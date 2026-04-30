#pragma once

#include "wifi_state_machine.h"
#include "wifi_events.h"

void wifi_manager_init(const wifi_callbacks_t *callbacks);
void wifi_manager_try_connect(const char *ssid, const char *pass);

int wifi_get_ap_client_count(void);
wifi_state_t wifi_get_state(void);
