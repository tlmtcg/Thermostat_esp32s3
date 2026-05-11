#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void serial_manager_init(void);
void handle_command(const char *cmd);
void serial_task(void *arg);
void do_time_status_dump(const char *arg);

#ifdef __cplusplus
}
#endif

