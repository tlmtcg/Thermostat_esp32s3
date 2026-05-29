#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SAISON_HIVER = 0,
    SAISON_INTERSAISON = 1,
    SAISON_ETE = 2
} saison_t;

void saison_update_text(float Text_now);
void saison_update(void);
float saison_get_Text_avg_48h(void);

void saison_save_profile(void);
void saison_load_profile(void);

#ifdef __cplusplus
}
#endif
