#ifndef EMAIL_SERVICE_H
#define EMAIL_SERVICE_H

#include <stdbool.h>

bool email_service_init(void);

bool email_send_async(
    const char *to,
    const char *subject,
    const char *body);

bool email_send_log_async(
    const char *to,
    const char *subject,
    const char *body,
    const char *filepath);

#endif
