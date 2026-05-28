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

// tau_ch = inertie chaudière + eau + radiateurs
const float tau_ch = 180.0f; // 3 minutes, à ajuster

// t = temps simulé depuis le début du chauffage
// float P_effective = P * (1.0f - expf(-t / tau_ch));
// Utilise P_effective au lieu de P

static const char *TAG = "PRED_ENGINE";
thermal_runtime_t g_thermal_runtime = {0};
static int64_t s_last_ekf_ts = 0;

// ======================================================
// Variables internes pour l’apprentissage 2R2C
// ======================================================
static float s_last_Tint = NAN;
static int64_t s_last_ts = 0;
static float s_dt = 0.5f; // valeur par défaut (1/2 seconde)

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

    float Ta_prev = Tint_prev;
    float Ta_now  = Tint_now;
    float Tm_prev = g_thermal_runtime.Tm;

    float Ra = prm.Ra;
    float Rm = prm.Rm;
    float Ca = prm.Ca;
    float Cm = prm.Cm;
    float P  = prm.P;

    float dT_meas = Ta_now - Ta_prev;
    float dTdt    = dT_meas / dt;

    g_thermal_runtime.last_dT   = dT_meas;
    g_thermal_runtime.last_dTdt = dTdt;

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_LEARN",
             "PARAMS BEFORE: Ra=%.3f Rm=%.3f Ca=%.0f Cm=%.0f P=%.0f",
             Ra, Rm, Ca, Cm, P);
#endif

    /* ============================================================
       TERMES DU MODELE 2R2C
       ============================================================ */
    float term_pertes = (Text_now - Ta_prev) / (Ra * Ca);
    float term_murs   = (Tm_prev - Ta_prev) / (Rm * Ca);

    /* ============================================================
       1) APPRENTISSAGE DE P (chauffage ON)
       ============================================================ */
    if (u > 0.5f)
    {
        float P_est = (dTdt - term_pertes - term_murs) * Ca;
        g_thermal_runtime.last_P_est = P_est;

#ifdef ENABLE_2R2C_DEBUG
        ESP_LOGI("2R2C_LEARN",
                 "HEAT ON: dTdt=%.6f pertes=%.6f murs=%.6f P_est=%.1f",
                 dTdt, term_pertes, term_murs, P_est);
#endif

        const float alpha_P = 0.001f;
        if (P_est > 100 && P_est < 5000)
            P = (1 - alpha_P) * P + alpha_P * P_est;
    }

    /* ============================================================
       2) APPRENTISSAGE DE Ra (chauffage OFF)
       ============================================================ */
    else
    {
        float denom = dTdt - term_murs;

        if (fabsf(denom) > 1e-7f)
        {
            float Ra_est = (Text_now - Ta_prev) / (Ca * denom);
            g_thermal_runtime.last_Ra_est = Ra_est;

#ifdef ENABLE_2R2C_DEBUG
            ESP_LOGI("2R2C_LEARN",
                     "HEAT OFF: dTdt=%.6f murs=%.6f Ra_est=%.3f",
                     dTdt, term_murs, Ra_est);
#endif

            const float alpha_R = 0.001f;
            if (Ra_est > 0.2f && Ra_est < 10.0f)
                Ra = (1 - alpha_R) * Ra + alpha_R * Ra_est;
        }
    }

    /* ============================================================
       3) APPRENTISSAGE DE Ca (toujours)
       ============================================================ */
    if (fabsf(dTdt) > 0.00005f)
    {
        float Ca_est = P / (dTdt - term_pertes - term_murs);

        const float alpha_C = 0.0002f;  // très lent
        if (Ca_est > 5000 && Ca_est < 300000)
            Ca = (1 - alpha_C) * Ca + alpha_C * Ca_est;
    }

    /* ============================================================
       4) APPRENTISSAGE DE Cm (via dynamique Ta↔Tm)
       ============================================================ */
    float dTm_est = (Ta_prev - Tm_prev) / (Rm * Cm);
    float Cm_est = (Ta_prev - Tm_prev) / (Rm * dTm_est + 1e-9f);

    const float alpha_Cm = 0.00005f;  // très lent
    if (Cm_est > 1e6 && Cm_est < 2e7)
        Cm = (1 - alpha_Cm) * Cm + alpha_Cm * Cm_est;

    /* ============================================================
       CLAMPS PHYSIQUES
       ============================================================ */
    if (Ca < 5000)     Ca = 5000;
    if (Ca > 300000)   Ca = 300000;
    if (Cm < 1e6)      Cm = 1e6;
    if (Cm > 2e7)      Cm = 2e7;
    if (P < 200)       P = 200;
    if (P > 5000)      P = 5000;
    if (Ra < 0.2f)     Ra = 0.2f;
    if (Ra > 10.0f)    Ra = 10.0f;

    prm.Ra = Ra;
    prm.Rm = Rm;
    prm.Ca = Ca;
    prm.Cm = Cm;
    prm.P  = P;

    thermal_2r2c_set_params(&prm);

#ifdef ENABLE_2R2C_DEBUG
    ESP_LOGI("2R2C_LEARN",
             "PARAMS AFTER: Ra=%.3f Rm=%.3f Ca=%.0f Cm=%.0f P=%.0f",
             Ra, Rm, Ca, Cm, P);
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

void prediction_engine_update(float Ta_mesuree, float Puissance_chauffage, float dt)
{
    // 1. Récupération des paramètres actuels
    float Ca = g_thermal_runtime.Ca;
    float Cm = g_thermal_runtime.Cm;
    float Ra = g_thermal_runtime.Ra;
    float Rm = g_thermal_runtime.Rm;

    if (Ca <= 0.0f || Cm <= 0.0f || Ra <= 0.0f || Rm <= 0.0f || dt <= 0.0f)
        return;

    // 2. Mise à jour de la matrice de transition F (discrétisation Euler)
    // dTa/dt = -Ta*(1/(Ra*Ca) + 1/(Rm*Ca)) + Tm/(Rm*Ca) + P/Ca
    // dTm/dt = Ta/(Rm*Cm) - Tm/(Rm*Cm)
    float a11 = 1.0f - dt * (1.0f / (Ra * Ca) + 1.0f / (Rm * Ca));
    float a12 = dt * (1.0f / (Rm * Ca));
    float a21 = dt * (1.0f / (Rm * Cm));
    float a22 = 1.0f - dt * (1.0f / (Rm * Cm));

    g_thermal_runtime.ekf_F[0][0] = a11;
    g_thermal_runtime.ekf_F[0][1] = a12;
    g_thermal_runtime.ekf_F[1][0] = a21;
    g_thermal_runtime.ekf_F[1][1] = a22;

    // 3. État courant x = [Ta; Tm]
    float Ta = g_thermal_runtime.Ta;
    float Tm = g_thermal_runtime.Tm;

    // 4. Prédiction de l'état : x_pred = F * x + B * u
    // B = [dt/Ca; 0] (on injecte la puissance dans Ta uniquement)
    float B0 = dt / Ca;
    float u = Puissance_chauffage;

    float Ta_pred = a11 * Ta + a12 * Tm + B0 * u;
    float Tm_pred = a21 * Ta + a22 * Tm;

    // 5. Prédiction de la covariance : P_pred = F * P * F' + Q
    float (*P)[2] = g_thermal_runtime.ekf_P;
    float (*Q)[2] = g_thermal_runtime.ekf_Q;

    float P00 = P[0][0], P01 = P[0][1];
    float P10 = P[1][0], P11 = P[1][1];

    // F * P
    float FP00 = a11 * P00 + a12 * P10;
    float FP01 = a11 * P01 + a12 * P11;
    float FP10 = a21 * P00 + a22 * P10;
    float FP11 = a21 * P01 + a22 * P11;

    // P_pred = (F * P) * F' + Q
    float Pp00 = FP00 * a11 + FP01 * a12 + Q[0][0];
    float Pp01 = FP00 * a21 + FP01 * a22 + Q[0][1];
    float Pp10 = FP10 * a11 + FP11 * a12 + Q[1][0];
    float Pp11 = FP10 * a21 + FP11 * a22 + Q[1][1];

    // 6. Observation : z = Ta_mesuree, H = [1 0]
    float H0 = g_thermal_runtime.ekf_H[0][0]; // devrait être 1.0
    float H1 = g_thermal_runtime.ekf_H[0][1]; // devrait être 0.0

    // Innovation : y = z - H * x_pred
    float z = Ta_mesuree;
    float z_pred = H0 * Ta_pred + H1 * Tm_pred;
    float y = z - z_pred;

    // 7. Innovation covariance : S = H * P_pred * H' + R
    // Comme H = [1 0], H*P_pred*H' = Pp00
    float R_meas = 0.05f; // bruit de mesure (à ajuster)
    float S = Pp00 * H0 * H0 + R_meas;
    if (S < 1e-6f)
        S = 1e-6f;

    // 8. Gain de Kalman : K = P_pred * H' * S^-1
    // H' = [1; 0]
    float K0 = Pp00 * H0 / S; // ligne Ta
    float K1 = Pp10 * H0 / S; // ligne Tm

    g_thermal_runtime.ekf_K[0][0] = K0;
    g_thermal_runtime.ekf_K[1][0] = K1;

    // 9. Mise à jour de l'état : x_new = x_pred + K * y
    Ta = Ta_pred + K0 * y;
    Tm = Tm_pred + K1 * y;

    g_thermal_runtime.Ta = Ta;
    g_thermal_runtime.Tm = Tm;

    // 10. Mise à jour de la covariance : P_new = (I - K*H) * P_pred
    // I - K*H = [[1-K0*H0, -K0*H1],
    //            [-K1*H0, 1-K1*H1]]
    float I00 = 1.0f - K0 * H0;
    float I01 = -K0 * H1;
    float I10 = -K1 * H0;
    float I11 = 1.0f - K1 * H1;

    float Pn00 = I00 * Pp00 + I01 * Pp10;
    float Pn01 = I00 * Pp01 + I01 * Pp11;
    float Pn10 = I10 * Pp00 + I11 * Pp10;
    float Pn11 = I10 * Pp01 + I11 * Pp11;

    P[0][0] = Pn00;
    P[0][1] = Pn01;
    P[1][0] = Pn10;
    P[1][1] = Pn11;

    // 11. Formatage des matrices pour l'affichage Web / JSON
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
    if (s_last_ts > 0)
    {
        s_dt = (float)(now_ts - s_last_ts);
        if (s_dt < 0.1f)
            s_dt = 0.1f; // sécurité
        if (s_dt > 600.0f)
            s_dt = 600.0f; // sécurité
    }

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

    // 1) Prédiction + update état 2R2C
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
#endif
            thermal_learn_step(s_last_Tint, Tint_now, Text_now, u, dt);
        }
#ifdef ENABLE_2R2C_DEBUG
        else
        {
            ESP_LOGW("2R2C_DBG", "LEARN SKIP: dt=%.2f (hors plage)", dt);
        }
#endif
    }
#ifdef ENABLE_2R2C_DEBUG
    else
    {
        ESP_LOGI("2R2C_DBG",
                 "LEARN SKIP: last_Tint/last_ts invalid (last_Tint=%.2f last_ts=%lld)",
                 s_last_Tint, (long long)s_last_ts);
    }
#endif

    // Mise à jour des références pour le prochain tick (pour le learn)
    s_last_Tint = Tint_now;
    s_last_ts = now_ts;

    // 3) Récup état + params 2R2C
    // thermal_state_t st;
    // thermal_2r2c_get_state(&st);

    // g_thermal_runtime.Ta = st.Ta;
    // g_thermal_runtime.Tm = st.Tm;

    // thermal_params_t prm;
    // thermal_2r2c_get_params(&prm);

    // g_thermal_runtime.Ta = st.Ta;
    // g_thermal_runtime.Tm = st.Tm;
    // g_thermal_runtime.Ra = prm.Ra;
    // g_thermal_runtime.Rm = prm.Rm;
    // g_thermal_runtime.Ca = prm.Ca;
    // g_thermal_runtime.Cm = prm.Cm;
    // g_thermal_runtime.P = prm.P;

    // 3bis) Tick EKF : on met à jour le filtre avec Ta mesurée + puissance
    if (s_last_ekf_ts > 0)
    {
        float dt_ekf = (float)(now_ts - s_last_ekf_ts);
        if (dt_ekf > 0.1f && dt_ekf < 600.0f)
        {
            float P_chauffage = u * g_thermal_runtime.P; // W
            prediction_engine_update(Tint_now, P_chauffage, dt_ekf);
        }
    }
    s_last_ekf_ts = now_ts;

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
             g_thermal_runtime.Ta, g_thermal_runtime.Tm, g_thermal_runtime.Ra, g_thermal_runtime.Rm, g_thermal_runtime.Ca, g_thermal_runtime.Cm, g_thermal_runtime.P);
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
