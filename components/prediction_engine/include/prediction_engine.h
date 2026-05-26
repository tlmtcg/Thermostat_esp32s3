#ifndef PREDICTION_ENGINE_H
#define PREDICTION_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float temp_ext_now;
    float temp_ext_1h;
    float temp_ext_3h;
    float temp_ext_6h;

    float humidity_now;
    float humidity_1h;

    int weather_code_now;
    int weather_code_1h;

} prediction_inputs_t;

typedef struct
{
    float Tint_1h;
    float Tint_3h;
    float Tint_6h;

    float weather_effect;
    float humidity_effect;
    float trend_effect;

    float heating_need_score; // -1 = refroidissement, +1 = besoin de chauffe
} prediction_outputs_t;

typedef struct {
    float Ta;
    float Tm;

    float time_to_reach;        // secondes
    int64_t start_heating_at;   // timestamp UNIX

    float Ra, Rm, Ca, Cm, P;    // paramètres thermiques
} thermal_runtime_t;

extern thermal_runtime_t g_thermal_runtime;

void prediction_engine_init(void);

char *prediction_engine_get_json_status(void);

void prediction_engine_tick(void);

#endif

