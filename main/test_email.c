#include "sd_card.h"
#include "email_service.h"

void test_email(void)
{

    email_service_init();

    email_send_log_async(
        "dup.cordon@gmail.com",
        "ESP32 Logs",
        "Logs en pièce jointe",
        MOUNT_POINT "/alerts.log");
}