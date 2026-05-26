#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------
// ÉTATS (air + murs)
// -----------------------------
typedef struct {
    float Ta;   // Température air
    float Tm;   // Température murs
} thermal_state_t;

// -----------------------------
// PARAMÈTRES THERMIQUES (2R2C)
// -----------------------------
typedef struct {
    float Ra;   // Résistance air -> extérieur
    float Rm;   // Résistance air -> murs
    float Ca;   // Capacité thermique air
    float Cm;   // Capacité thermique murs
    float P;    // Puissance chauffage
} thermal_params_t;

// -----------------------------
// API PUBLIQUE (stable)
// -----------------------------

// Initialise le modèle (EKF interne)
void thermal_2r2c_init(float dt_seconds, float Ta0);

// Mise à jour EKF avec mesure Ta
void thermal_2r2c_update(float Ta_measured);

// Prédiction 1 pas (Text, u)
void thermal_2r2c_predict(float Text, float u);

// Récupère Ta/Tm estimés
void thermal_2r2c_get_state(thermal_state_t *s);

// Récupère Ra/Rm/Ca/Cm/P estimés
void thermal_2r2c_get_params(thermal_params_t *p);

// Force des paramètres (optionnel)
void thermal_2r2c_set_params(const thermal_params_t *p);

float thermal_2r2c_time_to_reach(float Ta_target, float Text);

float thermal_2r2c_get_dt(void);

#ifdef __cplusplus
}
#endif
