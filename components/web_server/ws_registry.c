#include "ws_registry.h"

#include "ws_static.h"
#include "ws_api_wifi.h"
#include "ws_api_time.h"
#include "ws_api_led.h"
#include "ws_api_weather.h"
// #include "ws_logs.h"
// #include "ws_api_freebox.h"
// #include "ws_api_sys.h"

static ws_register_fn_t registry[] = {
    ws_register_static,
    ws_register_wifi_api,
    ws_register_time_api,
    ws_register_led_api,
    ws_register_weather_api,
    // ws_register_logs_api,
    // ws_register_freebox_api,
    // ws_register_sys_api
};

void ws_register_all(httpd_handle_t server)
{
    for (int i = 0; i < sizeof(registry) / sizeof(registry[0]); i++)
        registry[i](server);
}

