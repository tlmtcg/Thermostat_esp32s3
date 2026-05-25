#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float Ta;   // air
    float Tm;   // murs
} thermal_state_t;

typedef struct {
    float Ra;   // résistance air-ext
    float Rm;   // résistance air-murs
    float Ca;   // capacité air
    float Cm;   // capacité murs
    float P;    // puissance chauffage
} thermal_params_t;

void thermal_2r2c_set_params(const thermal_params_t *p);
void thermal_2r2c_init(float dt_seconds, float Ta0);
void thermal_2r2c_update(float Ta_measured);
void thermal_2r2c_get_state(thermal_state_t *s);
void thermal_2r2c_get_params(thermal_params_t *p);
void thermal_2r2c_predict(float Text, float u);

#ifdef __cplusplus
}
#endif
