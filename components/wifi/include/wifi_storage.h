#pragma once

#include <stdbool.h>
#include <stddef.h>

bool wifi_storage_load(char *ssid, size_t ssid_len,
                       char *pass, size_t pass_len);

bool wifi_storage_save(const char *ssid, const char *pass);
