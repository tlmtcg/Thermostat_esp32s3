#include "prediction_engine.h"
#include "esp_log.h"
#include <math.h>
#include "cJSON.h"
#include "thermostat.h"
#include "weather.h"
#include "thermal_engine.h"
#include "relay.h"
#include "heating_program.h"
#include "time.h"
#include "time_utils.h"
#include <math.h>

#define ENABLE_2R2C_DEBUG 0

static const char *TAG = "PRED_ENGINE";
thermal_runtime_t g_thermal_runtime = {0};

// ======================================================
// Variables internes pour l’apprentissage 2R2C
// ======================================================
static float s_last_Tint = NAN;
static int64_t s_last_ts = 0;

static float clamp(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void thermal_learn_step(float Tint_prev,
                               float Tint_now,
                               float Text_now,
                               float u,
                               float dt)
{
#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_LEARN",
             "ENTER: Tint_prev=%.2f Tint_now=%.2f Text=%.2f u=%.2f dt=%.2f",
             Tint_prev, Tint_now, Text_now, u, dt);
#endif

    thermal_params_t prm;
    thermal_2r2c_get_params(&prm);

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_LEARN",
             "PARAMS BEFORE: Ra=%.2f Ca=%.0f P=%.0f",
             prm.Ra, prm.Ca, prm.P);
#endif

    float dT_meas = Tint_now - Tint_prev;
    float dTdt = dT_meas / dt;

    // On mémorise systématiquement dT et dTdt dans le runtime
    g_thermal_runtime.last_dT = dT_meas;
    g_thermal_runtime.last_dTdt = dTdt;

    if (u > 0.5f)
    {
        // -------------------------
        // CHAUFFAGE ON → estimation P
        // -------------------------
        float term_pertes = (Tint_prev - Text_now) / (prm.Ra * prm.Ca);
        float P_est = (dTdt + term_pertes) * prm.Ca;

#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C_LEARN",
                 "HEAT ON: dT=%.4f dTdt=%.6f term_pertes=%.6f P_est=%.1f",
                 dT_meas, dTdt, term_pertes, P_est);
#endif

        // On mémorise la dernière estimation brute
        g_thermal_runtime.last_P_est = P_est;

        const float alpha_P = 0.001f;
        if (P_est > 100.0f && P_est < 5000.0f)
        {
            prm.P = (1.0f - alpha_P) * prm.P + alpha_P * P_est;
#ifdef ENABLE_2R2C_DEBUG

            ESP_LOGI("2R2C_LEARN", "P UPDATED → %.1f", prm.P);
#endif
        }
        else
        {
#ifdef ENABLE_2R2C_DEBUG
            ESP_LOGW("2R2C_LEARN", "P_est hors bornes: %.1f", P_est);
#endif
        }
    }
    else
    {
        // -------------------------
        // CHAUFFAGE OFF → estimation Ra
        // -------------------------
        if (fabsf(dTdt) > 0.0001f)
        {
            float Ra_est = -(Tint_prev - Text_now) / (dTdt * prm.Ca);

#ifdef ENABLE_2R2C_DEBUG
            ESP_LOGI("2R2C_LEARN",
                     "HEAT OFF: dT=%.4f dTdt=%.6f Ra_est=%.3f",
                     dT_meas, dTdt, Ra_est);
#endif

            // On mémorise la dernière estimation brute
            g_thermal_runtime.last_Ra_est = Ra_est;

            const float alpha_R = 0.001f;
            if (Ra_est > 0.2f && Ra_est < 5.0f)
            {
                prm.Ra = (1.0f - alpha_R) * prm.Ra + alpha_R * Ra_est;
#ifdef ENABLE_2R2C_DEBUG
                ESP_LOGI("2R2C_LEARN", "Ra UPDATED → %.3f", prm.Ra);
#endif
            }
            else
            {
#ifdef ENABLE_2R2C_DEBUG
                ESP_LOGW("2R2C_LEARN", "Ra_est hors bornes: %.3f", Ra_est);
#endif
            }
        }
        else
        {
#ifdef ENABLE_2R2C_DEBUG

            ESP_LOGW("2R2C_LEARN", "dTdt trop faible: %.6f", dTdt);
#endif
        }
    }

    // Clamp
    if (prm.Ca < 5000.0f)
        prm.Ca = 5000.0f;
    if (prm.Ca > 80000.0f)
        prm.Ca = 80000.0f;
    if (prm.P < 200.0f)
        prm.P = 200.0f;
    if (prm.P > 4000.0f)
        prm.P = 4000.0f;
    if (prm.Ra < 0.2f)
        prm.Ra = 0.2f;
    if (prm.Ra > 5.0f)
        prm.Ra = 5.0f;

    thermal_2r2c_set_params(&prm);

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_LEARN",
             "PARAMS AFTER: Ra=%.2f Ca=%.0f P=%.0f",
             prm.Ra, prm.Ca, prm.P);
#endif
}

char *prediction_engine_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    // Toujours renvoyer "ok" tant que le modèle thermique tourne
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddBoolToObject(root, "enabled_2R2C", g_thermostat_runtime.enable_2r2c);

    // États thermiques
    cJSON_AddNumberToObject(root, "Ta", g_thermal_runtime.Ta);
    cJSON_AddNumberToObject(root, "Tm", g_thermal_runtime.Tm);

    // Paramètres du modèle
    cJSON_AddNumberToObject(root, "Ra", g_thermal_runtime.Ra);
    cJSON_AddNumberToObject(root, "Rm", g_thermal_runtime.Rm);
    cJSON_AddNumberToObject(root, "Ca", g_thermal_runtime.Ca);
    cJSON_AddNumberToObject(root, "Cm", g_thermal_runtime.Cm);
    cJSON_AddNumberToObject(root, "P", g_thermal_runtime.P);

    // Chauffe prédictive
    cJSON_AddNumberToObject(root, "time_to_reach", g_thermal_runtime.time_to_reach);
    cJSON_AddNumberToObject(root, "start_heating_at", g_thermal_runtime.start_heating_at);

    // Consignes
    cJSON_AddNumberToObject(root, "effective_consigne", g_thermostat_runtime.effective_consigne);
    cJSON_AddNumberToObject(root, "next_consigne", g_thermostat_runtime.next_consigne);
    cJSON_AddNumberToObject(root, "next_consigne_ts", g_thermostat_runtime.next_consigne_ts);

    // Estimations
    cJSON_AddNumberToObject(root, "last_dT", g_thermal_runtime.last_dT);
    cJSON_AddNumberToObject(root, "last_dTdt", g_thermal_runtime.last_dTdt);
    cJSON_AddNumberToObject(root, "last_Ra_est", g_thermal_runtime.last_Ra_est);
    cJSON_AddNumberToObject(root, "last_P_est", g_thermal_runtime.last_P_est);

    // --- Matrices EKF (Filtre de Kalman Étendu) ---
    // Ajout sous forme de chaînes formatées/brutes pour alimenter les balises <pre> du mode expert
    cJSON_AddStringToObject(root, "matrix_F", g_thermal_runtime.ekf_matrix_F_str);
    cJSON_AddStringToObject(root, "matrix_P", g_thermal_runtime.ekf_matrix_P_str);
    cJSON_AddStringToObject(root, "matrix_Q", g_thermal_runtime.ekf_matrix_Q_str);
    cJSON_AddStringToObject(root, "matrix_H", g_thermal_runtime.ekf_matrix_H_str);
    cJSON_AddStringToObject(root, "matrix_K", g_thermal_runtime.ekf_matrix_K_str);

    // Conversion JSON → string
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json; // libéré par l’API web
}

void prediction_engine_tick(void)
{
    // 1) Prochaine consigne programmée
    int64_t ts = heating_program_get_next_target_timestamp();
    g_thermostat_runtime.next_consigne_ts = ts;

    if (ts > 0)
    {
        struct tm nt = time_utils_localtime_from_ts(ts);
        uint32_t sec_midnight = nt.tm_hour * 3600 + nt.tm_min * 60 + nt.tm_sec;

        g_thermostat_runtime.next_consigne =
            heating_get_temp((nt.tm_wday + 6) % 7, sec_midnight);
    }
    else
    {
        g_thermostat_runtime.next_consigne = -1;
    }

    float Tint_now = g_thermostat_runtime.temperature;
    float Text_now = weather_get_forecast_temp(0);
    float u = get_relay_state() ? 1.0f : 0.0f;
    int64_t now_ts = time(NULL);

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_DBG",
             "TICK: now_ts=%lld ts=%lld Tint=%.2f Text=%.2f u=%.2f next=%.2f next_ts=%lld last_Tint=%.2f last_ts=%lld",
             (long long)now_ts,
             (long long)ts,
             Tint_now,
             Text_now,
             u,
             g_thermostat_runtime.next_consigne,
             (long long)g_thermostat_runtime.next_consigne_ts,
             s_last_Tint,
             (long long)s_last_ts);
#endif

    if (Tint_now < -50 || Tint_now > 80)
    {
#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGW("2R2C_DBG", "Temp intérieure invalide, tick ignoré");
#endif
        return;
    }

    // 1) Prédiction + update état
    thermal_2r2c_predict(Text_now, u);
    thermal_2r2c_update(Tint_now);

    // 2) Apprentissage (si on a une mesure précédente)
    if (!isnan(s_last_Tint) && s_last_ts > 0)
    {
        float dt = (float)(now_ts - s_last_ts);

#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C_DBG",
                 "LEARN CANDIDATE: last_Tint=%.2f Tint_now=%.2f dt=%.2f u=%.2f",
                 s_last_Tint, Tint_now, dt, u);
#endif

        if (dt > 0.5f && dt < 600.0f) // entre 0.5s et 10 min
        {
#ifdef ENABLE_2R2C_DEBUG

            ESP_LOGI("2R2C_DBG", "LEARN CALL: dt=%.2f", dt);
            thermal_learn_step(s_last_Tint, Tint_now, Text_now, u, dt);
#endif
        }
        else
        {
#ifdef ENABLE_2R2C_DEBUG
            ESP_LOGW("2R2C_DBG", "LEARN SKIP: dt=%.2f (hors plage)", dt);
#endif
        }
    }
    else
    {
#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C_DBG",
                 "LEARN SKIP: last_Tint/last_ts invalid (last_Tint=%.2f last_ts=%lld)",
                 s_last_Tint, (long long)s_last_ts);
#endif
    }

    // Mise à jour des références pour le prochain tick
    s_last_Tint = Tint_now;
    s_last_ts = now_ts;

    // 3) Récup état + params
    thermal_state_t st;
    thermal_2r2c_get_state(&st);

    g_thermal_runtime.Ta = st.Ta;
    g_thermal_runtime.Tm = st.Tm;

    thermal_params_t prm;
    thermal_2r2c_get_params(&prm);

    g_thermal_runtime.Ra = prm.Ra;
    g_thermal_runtime.Rm = prm.Rm;
    g_thermal_runtime.Ca = prm.Ca;
    g_thermal_runtime.Cm = prm.Cm;
    g_thermal_runtime.P = prm.P;

    /* =========================================================
       FORMATAGE DES MATRICES POUR L'EXPORT JSON WEB
       ========================================================= */
    snprintf(g_thermal_runtime.ekf_matrix_F_str, sizeof(g_thermal_runtime.ekf_matrix_F_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_F[0][0], g_thermal_runtime.ekf_F[0][1],
             g_thermal_runtime.ekf_F[1][0], g_thermal_runtime.ekf_F[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_P_str, sizeof(g_thermal_runtime.ekf_matrix_P_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_P[0][0], g_thermal_runtime.ekf_P[0][1],
             g_thermal_runtime.ekf_P[1][0], g_thermal_runtime.ekf_P[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_K_str, sizeof(g_thermal_runtime.ekf_matrix_K_str),
             "[[%.4f], [%.4f]]",
             g_thermal_runtime.ekf_K[0][0],
             g_thermal_runtime.ekf_K[1][0]);

    snprintf(g_thermal_runtime.ekf_matrix_Q_str, sizeof(g_thermal_runtime.ekf_matrix_Q_str),
             "[[%.5f, %.5f], [%.5f, %.5f]]",
             g_thermal_runtime.ekf_Q[0][0], g_thermal_runtime.ekf_Q[0][1],
             g_thermal_runtime.ekf_Q[1][0], g_thermal_runtime.ekf_Q[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_H_str, sizeof(g_thermal_runtime.ekf_matrix_H_str),
             "[[%.1f, %.1f]]",
             g_thermal_runtime.ekf_H[0][0], g_thermal_runtime.ekf_H[0][1]);

    // 4) Choix de la consigne pour le modèle
    float consigne = g_thermostat_runtime.effective_consigne;

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C",
             "PREDICT: Tint=%.2f Text=%.2f consigne=%.2f next=%.2f next_ts=%lld",
             Tint_now, Text_now, consigne,
             g_thermostat_runtime.next_consigne,
             (long long)g_thermostat_runtime.next_consigne_ts);
#endif

    // Cas 1 : déjà au-dessus de la consigne → pas besoin de chauffer
    if (!isnan(consigne) && Tint_now >= consigne)
    {
        g_thermal_runtime.time_to_reach = 0;
        g_thermal_runtime.start_heating_at = -1;
#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C",
                 "Déjà au-dessus de la consigne (Tint=%.2f >= %.2f) → t2r=0, pas d’early-start",
                 Tint_now, consigne);
#endif
    }
    else
    {
        // Cas 2 : on est en dessous → on demande au modèle
        g_thermal_runtime.time_to_reach =
            thermal_2r2c_time_to_reach(consigne, Text_now);

        if (g_thermal_runtime.time_to_reach > 0 && ts > 0)
        {
            g_thermal_runtime.start_heating_at =
                ts - (int64_t)g_thermal_runtime.time_to_reach;
        }
        else
        {
            g_thermal_runtime.start_heating_at = -1;
        }
#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C",
                 "t2r=%.0f start_at=%lld (target_ts=%lld)",
                 g_thermal_runtime.time_to_reach,
                 (long long)g_thermal_runtime.start_heating_at,
                 (long long)ts);
#endif
    }
#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C",
             "Ta=%.2f Tm=%.2f Ra=%.2f Rm=%.2f Ca=%.0f Cm=%.0f P=%.0f",
             st.Ta, st.Tm, prm.Ra, prm.Rm, prm.Ca, prm.Cm, prm.P);
#endif
}

void prediction_engine_init(void)
{
    // Initialisation des états de base
    g_thermal_runtime.Ta = 20.0f;
    g_thermal_runtime.Tm = 19.0f;

    // --- INITIALISATION EKF ---
    // P : Incertitude initiale (diagonale)
    g_thermal_runtime.ekf_P[0][0] = 1.0f; // Incertitude sur Ta
    g_thermal_runtime.ekf_P[0][1] = 0.0f;
    g_thermal_runtime.ekf_P[1][0] = 0.0f;
    g_thermal_runtime.ekf_P[1][1] = 5.0f; // Plus d'incertitude sur Tm (non mesurée)

    // Q : Bruit de processus (confiance dans les équations du modèle thermique)
    g_thermal_runtime.ekf_Q[0][0] = 0.001f;
    g_thermal_runtime.ekf_Q[0][1] = 0.0f;
    g_thermal_runtime.ekf_Q[1][0] = 0.0f;
    g_thermal_runtime.ekf_Q[1][1] = 0.0001f;

    // H : Matrice d'observation (on ne mesure directement que Ta, pas Tm)
    g_thermal_runtime.ekf_H[0][0] = 1.0f;
    g_thermal_runtime.ekf_H[0][1] = 0.0f;
}

void prediction_engine_update(float Ta_mesuree, float Puissance_chauffage, float dt)
{
    // 1. Récupération des paramètres actuels
    float Ca = g_thermal_runtime.Ca;
    float Cm = g_thermal_runtime.Cm;
    float Ra = g_thermal_runtime.Ra;
    float Rm = g_thermal_runtime.Rm;

    // 2. Mise à jour de la matrice de transition F (Jacobienne discrétisée par Euler)
    // dTa/dt = -Ta*(1/(Ra*Ca) + 1/(Rm*Ca)) + Tm/(Rm*Ca) + P/Ca
    // dTm/dt = Ta/(Rm*Cm) - Tm/(Rm*Cm)
    g_thermal_runtime.ekf_F[0][0] = 1.0f - dt * (1.0f / (Ra * Ca) + 1.0f / (Rm * Ca));
    g_thermal_runtime.ekf_F[0][1] = dt * (1.0f / (Rm * Ca));
    g_thermal_runtime.ekf_F[1][0] = dt * (1.0f / (Rm * Cm));
    g_thermal_runtime.ekf_F[1][1] = 1.0f - dt * (1.0f / (Rm * Cm));

    // [ ... Ici insérer tes calculs de prédiction et de correction Kalman (P = F*P*F' + Q, etc.) ... ]
    // [ ... Calcul du Gain de Kalman K ... ]

    // 3. Formater les matrices en chaîne à la toute fin de l'update pour l'affichage Web
    snprintf(g_thermal_runtime.ekf_matrix_F_str, sizeof(g_thermal_runtime.ekf_matrix_F_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_F[0][0], g_thermal_runtime.ekf_F[0][1],
             g_thermal_runtime.ekf_F[1][0], g_thermal_runtime.ekf_F[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_P_str, sizeof(g_thermal_runtime.ekf_matrix_P_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_P[0][0], g_thermal_runtime.ekf_P[0][1],
             g_thermal_runtime.ekf_P[1][0], g_thermal_runtime.ekf_P[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_K_str, sizeof(g_thermal_runtime.ekf_matrix_K_str),
             "[[%.4f], [%.4f]]",
             g_thermal_runtime.ekf_K[0][0],
             g_thermal_runtime.ekf_K[1][0]);

    // 3. Formater les matrices en chaîne pour l'affichage Web / JSON
    snprintf(g_thermal_runtime.ekf_matrix_F_str, sizeof(g_thermal_runtime.ekf_matrix_F_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_F[0][0], g_thermal_runtime.ekf_F[0][1],
             g_thermal_runtime.ekf_F[1][0], g_thermal_runtime.ekf_F[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_P_str, sizeof(g_thermal_runtime.ekf_matrix_P_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             g_thermal_runtime.ekf_P[0][0], g_thermal_runtime.ekf_P[0][1],
             g_thermal_runtime.ekf_P[1][0], g_thermal_runtime.ekf_P[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_K_str, sizeof(g_thermal_runtime.ekf_matrix_K_str),
             "[[%.4f], [%.4f]]",
             g_thermal_runtime.ekf_K[0][0],
             g_thermal_runtime.ekf_K[1][0]);

    // Complément pour Q (Matrice de bruit de processus 2x2)
    snprintf(g_thermal_runtime.ekf_matrix_Q_str, sizeof(g_thermal_runtime.ekf_matrix_Q_str),
             "[[%.5f, %.5f], [%.5f, %.5f]]",
             g_thermal_runtime.ekf_Q[0][0], g_thermal_runtime.ekf_Q[0][1],
             g_thermal_runtime.ekf_Q[1][0], g_thermal_runtime.ekf_Q[1][1]);

    // Complément pour H (Matrice d'observation 1x2)
    snprintf(g_thermal_runtime.ekf_matrix_H_str, sizeof(g_thermal_runtime.ekf_matrix_H_str),
             "[[%.1f, %.1f]]",
             g_thermal_runtime.ekf_H[0][0], g_thermal_runtime.ekf_H[0][1]);
}
