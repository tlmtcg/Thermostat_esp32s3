#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void serial_manager_init(void);
void handle_command(const char *cmd);
void serial_task(void *arg);

#ifdef __cplusplus
}
#endif

