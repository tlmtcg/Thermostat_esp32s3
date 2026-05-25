#ifndef PREDICT_ADJUSTMENT_H
#define PREDICT_ADJUSTMENT_H

#include "thermal_model.h"
#include "prediction_engine.h"

typedef struct
{
    float consigne_auto;     // consigne du programme horaire
    float Tint_now;          // température intérieure
    float Text_now;          // température extérieure
} predict_adjustment_inputs_t;

float predict_adjustment_compute(
    const thermal_model_t *model,
    const prediction_outputs_t *pred,
    const predict_adjustment_inputs_t *inputs);

#endif

