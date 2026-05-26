#include "predict_adjustment.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "PRED_ADJ";

static float clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// float predict_adjustment_compute(
//     const thermal_model_t *model,
//     const prediction_outputs_t *pred,
//     const predict_adjustment_inputs_t *inputs)
// {
//     if (!model || !pred || !inputs)
//         return 0.0f;

//     float adj = 0.0f;

//     // 1️⃣ Écart prévu entre Tint dans 1h et la consigne Auto
//     float delta_1h = inputs->consigne_auto - pred->Tint_1h;

//     // Si la maison va être trop froide → anticiper
//     if (delta_1h > 0.3f)
//         adj += clamp(delta_1h * 0.4f, 0.0f, 1.0f);

//     // Si la maison va être trop chaude → éviter la surchauffe
//     if (delta_1h < -0.3f)
//         adj += clamp(delta_1h * 0.4f, -1.0f, 0.0f);

//     // 2️⃣ Effet météo (ensoleillement, pluie, neige…)
//     adj += pred->weather_effect * 0.5f;

//     // 3️⃣ Effet humidité (sensation de froid)
//     adj += pred->humidity_effect * 0.3f;

//     // 4️⃣ Tendance météo (front froid / chaud)
//     adj += pred->trend_effect * 0.4f;

//     // 5️⃣ Score global de besoin de chauffe
//     adj += pred->heating_need_score * 0.6f;

//     // 6️⃣ On borne l’ajustement final
//     adj = clamp(adj, -1.5f, +1.5f);

//     ESP_LOGI(TAG,
//         "Predict adj: %.2f | delta1h=%.2f weather=%.2f hum=%.2f trend=%.2f score=%.2f",
//         adj, delta_1h,
//         pred->weather_effect,
//         pred->humidity_effect,
//         pred->trend_effect,
//         pred->heating_need_score);

//     return adj;
// }
