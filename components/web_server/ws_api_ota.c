#include "ws_api_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_app_format.h"
#include "string.h"

static const char *TAG = "WS_OTA";

#define OTA_BUFFER_SIZE 2048

typedef struct {
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    bool started;
    bool sha_init;
} ota_session_t;

static ota_session_t ota_session = {0};

static esp_err_t ota_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *partition = NULL;

    esp_err_t err;
    char buf[4096];
    int received;
    bool image_header_checked = false;

    ESP_LOGI(TAG, "Starting OTA");

    partition = esp_ota_get_next_update_partition(NULL);

    if (!partition)
    {
        ESP_LOGE(TAG, "No OTA partition");
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA partition");
        return ESP_FAIL;
    }

    err = esp_ota_begin(partition,
                        OTA_SIZE_UNKNOWN,
                        &ota_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "esp_ota_begin failed: %s",
                 esp_err_to_name(err));

        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA begin failed");

        return err;
    }

    int total = 0;

    while (1)
    {
        received = httpd_req_recv(req,
                                  buf,
                                  sizeof(buf));

        if (received == HTTPD_SOCK_ERR_TIMEOUT)
        {
            continue;
        }

        if (received <= 0)
        {
            break;
        }

        // Vérification MAGIC BYTE
        if (!image_header_checked)
        {
            image_header_checked = true;

            if ((uint8_t)buf[0] != 0xE9)
            {
                ESP_LOGE(TAG,
                         "Invalid firmware magic byte: 0x%02X",
                         (uint8_t)buf[0]);

                esp_ota_abort(ota_handle);

                httpd_resp_send_err(req,
                                    HTTPD_400_BAD_REQUEST,
                                    "Invalid firmware");

                return ESP_FAIL;
            }
        }

        err = esp_ota_write(ota_handle,
                            buf,
                            received);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG,
                     "ota_write failed: %s",
                     esp_err_to_name(err));

            esp_ota_abort(ota_handle);

            httpd_resp_send_err(req,
                                HTTPD_500_INTERNAL_SERVER_ERROR,
                                "write failed");

            return err;
        }

        total += received;
    }

    ESP_LOGI(TAG, "OTA received %d bytes", total);

    err = esp_ota_end(ota_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "esp_ota_end failed: %s",
                 esp_err_to_name(err));

        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA end failed");

        return err;
    }

    err = esp_ota_set_boot_partition(partition);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "set_boot_partition failed: %s",
                 esp_err_to_name(err));

        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Boot partition failed");

        return err;
    }

    ESP_LOGI(TAG, "OTA OK -> rebooting");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");

    httpd_resp_send(req,
                    "{\"status\":\"ok\"}",
                    HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));

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