#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void serial_manager_init(void);
void handle_command(const char *cmd);

#ifdef __cplusplus
}
#endif

