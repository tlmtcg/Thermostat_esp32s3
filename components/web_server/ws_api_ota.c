#include "ws_api_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_app_format.h"
#include "string.h"

static const char *TAG = "WS_OTA";

#define OTA_BUFFER_SIZE 2048

typedef struct {
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    bool started;
} ota_session_t;

static ota_session_t ota_session = {0};

static esp_err_t ota_update_handler(httpd_req_t *req)
{
    char buf[1024];
    int received;

    esp_err_t err;

    ESP_LOGI(TAG, "OTA request received, content length = %d", req->content_len);

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition)
    {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t handle;

    err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return err;
    }

    size_t total = 0;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0)
    {
        total += received;

        err = esp_ota_write(handle, buf, received);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return err;
        }
    }

    if (received < 0)
    {
        ESP_LOGE(TAG, "HTTP receive error");
        esp_ota_abort(handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA received %d bytes", total);

    err = esp_ota_end(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return err;
    }

    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot set failed");
        return err;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
        "{\"status\":\"ok\",\"reboot\":true}",
        HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "OTA success, rebooting...");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

esp_err_t ws_register_ota_api(httpd_handle_t server)
{
    httpd_uri_t uri_update = {
        .uri = "/api/ota/update",
        .method = HTTP_POST,
        .handler = ota_update_handler,
        .user_ctx = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_update));

    ESP_LOGI(TAG, "OTA update endpoint registered");

    return ESP_OK;
}