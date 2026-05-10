#include "email_service.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "mbedtls/base64.h"
#include "sdkconfig.h"

static const char *TAG = "EMAIL";

extern const uint8_t google_root_pem_start[] asm("_binary_google_root_pem_start");
extern const uint8_t google_root_pem_end[] asm("_binary_google_root_pem_end");

// ==========================================
// GMAIL CONFIG
// ==========================================

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define EMAIL_QUEUE_SIZE 10
#define EMAIL_TASK_STACK 16384

// ==========================================
// EMAIL STRUCT
// ==========================================

typedef struct
{
    char to[128];
    char subject[128];
    char body[1024];

    bool has_attachment;
    char filepath[256];

} email_msg_t;

// ==========================================
// QUEUE
// ==========================================

static QueueHandle_t email_queue = NULL;

// ==========================================
// SMTP HELPERS
// ==========================================

static bool smtp_write(struct esp_tls *tls, const char *data)
{
    int len = strlen(data);
    int ret = esp_tls_conn_write(tls, data, len);

    if (ret <= 0)
    {
        ESP_LOGE(TAG, "SMTP write failed (%d)", ret);
        return false;
    }

    return true;
}

static bool smtp_read_full(
    struct esp_tls *tls,
    const char *filepath)
{
    FILE *f = fopen(filepath, "rb");

    if (!f)
    {
        ESP_LOGE(TAG, "Cannot open file: %s", filepath);
        return false;
    }

    uint8_t raw[256];
    unsigned char b64[512];

    while (1)
    {
        size_t read_len = fread(raw, 1, sizeof(raw), f);

        if (read_len == 0)
            break;

        size_t olen = 0;

        int ret = mbedtls_base64_encode(
            b64,
            sizeof(b64),
            &olen,
            raw,
            read_len);

        if (ret != 0)
        {
            ESP_LOGE(TAG, "Base64 encode failed (%d)", ret);
            fclose(f);
            return false;
        }

        b64[olen] = 0;

        esp_tls_conn_write(tls, (char *)b64, olen);
        esp_tls_conn_write(tls, "\r\n", 2);
    }

    fclose(f);
    return true;
}

static int smtp_read(struct esp_tls *tls, char *buf, size_t len)
{
    memset(buf, 0, len);

    int ret = esp_tls_conn_read(tls, buf, len - 1);

    return ret;
}

static bool smtp_expect(struct esp_tls *tls, const char *expected)
{
    char rx[512];

    int len = smtp_read(tls, rx, sizeof(rx));

    if (len <= 0)
    {
        ESP_LOGE(TAG, "SMTP read failed");
        return false;
    }

    ESP_LOGI(TAG, "SMTP RX: %s", rx);

    return strstr(rx, expected) != NULL;
}

// ==========================================
// BASE64
// ==========================================

static void b64_encode(const char *input, char *output, size_t output_size)
{
    size_t olen = 0;

    mbedtls_base64_encode(
        (unsigned char *)output,
        output_size,
        &olen,
        (const unsigned char *)input,
        strlen(input));

    output[olen] = 0;
}

// ==========================================
// ATTACHMENT
// ==========================================

static bool smtp_send_attachment(struct esp_tls *tls, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");

    if (!f)
    {
        ESP_LOGE(TAG, "Cannot open attachment: %s", filepath);
        return false;
    }

    uint8_t raw[256];
    unsigned char b64[1024]; // 384 -> 512 max en base64

    while (1)
    {
        size_t read = fread(raw, 1, sizeof(raw), f);

        if (read == 0)
            break;

        size_t olen = 0;

        int ret = mbedtls_base64_encode(
            b64,
            sizeof(b64),
            &olen,
            raw,
            read);

        if (ret != 0)
        {
            ESP_LOGE(TAG, "Base64 encode failed (%d)", ret);
            fclose(f);
            return false;
        }

        b64[olen] = 0;

        if (!smtp_write(tls, (char *)b64))
        {
            fclose(f);
            return false;
        }

        if (!smtp_write(tls, "\r\n"))
        {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

// ==========================================
// SEND MAIL
// ==========================================

static bool smtp_send_email(const email_msg_t *msg)
{
    char tx[2048];

    char user_b64[256];
    char pass_b64[256];

    b64_encode(CONFIG_SMTP_USER, user_b64, sizeof(user_b64));
    b64_encode(CONFIG_SMTP_PASS, pass_b64, sizeof(pass_b64));

    esp_tls_cfg_t cfg = {
        .cacert_buf = google_root_pem_start,
        .cacert_bytes = google_root_pem_end - google_root_pem_start,
        .timeout_ms = 10000,
    };

    struct esp_tls *tls = esp_tls_init();
    if (!tls)
    {
        ESP_LOGE(TAG, "esp_tls_init failed");
        return false;
    }

    int ret = esp_tls_conn_new_sync(
        SMTP_HOST,
        strlen(SMTP_HOST),
        SMTP_PORT,
        &cfg,
        tls);

    if (ret != 1)
    {
        ESP_LOGE(TAG, "TLS connect failed (%d)", ret);
        esp_tls_conn_destroy(tls);
        return false;
    }

    ESP_LOGI(TAG, "TLS connected");

    // ======================================
    // SMTP GREETING
    // ======================================

    if (!smtp_expect(tls, "220"))
        goto error;

    if (!smtp_write(tls, "EHLO esp32\r\n"))
        goto error;

    if (!smtp_expect(tls, "250"))
        goto error;

    // ======================================
    // AUTH LOGIN
    // ======================================

    if (!smtp_write(tls, "AUTH LOGIN\r\n"))
        goto error;

    if (!smtp_expect(tls, "334"))
        goto error;

    snprintf(tx, sizeof(tx), "%s\r\n", user_b64);
    if (!smtp_write(tls, tx))
        goto error;

    if (!smtp_expect(tls, "334"))
        goto error;

    snprintf(tx, sizeof(tx), "%s\r\n", pass_b64);
    if (!smtp_write(tls, tx))
        goto error;

    if (!smtp_expect(tls, "235"))
        goto error;

    // ======================================
    // MAIL FROM
    // ======================================

    snprintf(tx, sizeof(tx), "MAIL FROM:<%s>\r\n", CONFIG_SMTP_USER);
    if (!smtp_write(tls, tx))
        goto error;

    if (!smtp_expect(tls, "250"))
        goto error;

    // ======================================
    // RCPT TO
    // ======================================

    snprintf(tx, sizeof(tx), "RCPT TO:<%s>\r\n", msg->to);
    if (!smtp_write(tls, tx))
        goto error;

    if (!smtp_expect(tls, "250"))
        goto error;

    // ======================================
    // DATA
    // ======================================

    if (!smtp_write(tls, "DATA\r\n"))
        goto error;

    if (!smtp_expect(tls, "354"))
        goto error;

    // ======================================
    // HEADERS
    // ======================================

    snprintf(
        tx,
        sizeof(tx),
        "From: ESP32 <%s>\r\n"
        "To: <%s>\r\n"
        "Subject: %s\r\n",
        CONFIG_SMTP_USER,
        msg->to,
        msg->subject);

    if (!smtp_write(tls, tx))
        goto error;

    // ======================================
    // MIME
    // ======================================

    if (msg->has_attachment)
    {
        if (!smtp_write(tls, "MIME-Version: 1.0\r\n"))
            goto error;

        if (!smtp_write(tls, "Content-Type: multipart/mixed; boundary=sep\r\n\r\n"))
            goto error;

        if (!smtp_write(tls, "--sep\r\n"))
            goto error;

        if (!smtp_write(tls, "Content-Type: text/plain; charset=UTF-8\r\n\r\n"))
            goto error;
    }
    else
    {
        if (!smtp_write(tls, "Content-Type: text/plain; charset=UTF-8\r\n\r\n"))
            goto error;
    }

    // ======================================
    // BODY
    // ======================================

    if (!smtp_write(tls, msg->body))
        goto error;

    if (!smtp_write(tls, "\r\n"))
        goto error;

    // ======================================
    // ATTACHMENT
    // ======================================

    if (msg->has_attachment)
    {
        if (!smtp_write(tls, "\r\n--sep\r\n"))
            goto error;

        if (!smtp_write(tls, "Content-Type: application/octet-stream\r\n"))
            goto error;

        if (!smtp_write(tls, "Content-Transfer-Encoding: base64\r\n"))
            goto error;

        if (!smtp_write(tls, "Content-Disposition: attachment; filename=\"log.txt\"\r\n\r\n"))
            goto error;

        if (!smtp_send_attachment(tls, msg->filepath))
            goto error;

        if (!smtp_write(tls, "\r\n--sep--\r\n"))
            goto error;
    }

    // ======================================
    // END DATA
    // ======================================

    if (!smtp_write(tls, "\r\n.\r\n"))
        goto error;

    if (!smtp_expect(tls, "250"))
        goto error;

    // ======================================
    // QUIT
    // ======================================

    smtp_write(tls, "QUIT\r\n");
    esp_tls_conn_destroy(tls);

    ESP_LOGI(TAG, "Mail sent");
    return true;

error:
    ESP_LOGE(TAG, "SMTP transaction failed");
    esp_tls_conn_destroy(tls);
    return false;
}

// ==========================================
// TASK
// ==========================================

static void smtp_task(void *pv)
{
    email_msg_t msg;

    while (1)
    {
        if (xQueueReceive(email_queue, &msg, portMAX_DELAY))
        {
            smtp_send_email(&msg);
        }
    }
}

// ==========================================
// INIT
// ==========================================

bool email_service_init(void)
{
    email_queue = xQueueCreate(EMAIL_QUEUE_SIZE, sizeof(email_msg_t));

    if (!email_queue)
    {
        ESP_LOGE(TAG, "Failed to create email queue");
        return false;
    }

    if (xTaskCreate(
            smtp_task,
            "smtp_task",
            EMAIL_TASK_STACK,
            NULL,
            5,
            NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create smtp_task");
        vQueueDelete(email_queue);
        email_queue = NULL;
        return false;
    }

    return true;
}

// ==========================================
// SIMPLE EMAIL
// ==========================================

bool email_send_async(const char *to, const char *subject, const char *body)
{
    if (!email_queue)
        return false;

    email_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    strncpy(msg.to, to, sizeof(msg.to) - 1);
    strncpy(msg.subject, subject, sizeof(msg.subject) - 1);
    strncpy(msg.body, body, sizeof(msg.body) - 1);

    return xQueueSend(email_queue, &msg, pdMS_TO_TICKS(100)) == pdPASS;
}

// ==========================================
// EMAIL + ATTACHMENT
// ==========================================

bool email_send_log_async(
    const char *to,
    const char *subject,
    const char *body,
    const char *filepath)
{
    if (!email_queue)
        return false;

    email_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    strncpy(msg.to, to, sizeof(msg.to) - 1);
    strncpy(msg.subject, subject, sizeof(msg.subject) - 1);
    strncpy(msg.body, body, sizeof(msg.body) - 1);
    strncpy(msg.filepath, filepath, sizeof(msg.filepath) - 1);

    msg.has_attachment = true;

    return xQueueSend(email_queue, &msg, pdMS_TO_TICKS(100)) == pdPASS;
}
