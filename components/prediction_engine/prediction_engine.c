#include "prediction_engine.h"
#include "esp_log.h"
#include "cJSON.h"
#include "thermostat.h"
#include "weather.h"
#include "thermal_engine.h"
#include "relay.h"
#include "heating_program.h"
#include "time.h"
#include "time_utils.h"
#include "math.h"
#include "saison_manager.h"

#define ENABLE_2R2C_DEBUG 0

// tau_ch = inertie chaudière + eau + radiateurs
const float tau_ch = 180.0f; // 3 minutes, à ajuster

// t = temps simulé depuis le début du chauffage
// float P_effective = P * (1.0f - expf(-t / tau_ch));
// Utilise P_effective au lieu de P

static const char *TAG = "PRED_ENGINE";
thermal_runtime_t g_thermal_runtime = {0};
thermal_model_t g_saved_model = {0};

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

bool model_changed_significantly(void)
{
    if (fabsf(g_thermal_runtime.Ra - g_saved_model.Ra) > g_saved_model.Ra * 0.05f) return true;
    if (fabsf(g_thermal_runtime.Rm - g_saved_model.Rm) > g_saved_model.Rm * 0.05f) return true;
    if (fabsf(g_thermal_runtime.Ca - g_saved_model.Ca) > g_saved_model.Ca * 0.10f) return true;
    if (fabsf(g_thermal_runtime.Cm - g_saved_model.Cm) > g_saved_model.Cm * 0.10f) return true;
    if (fabsf(g_thermal_runtime.P  - g_saved_model.P ) > g_saved_model.P  * 0.05f) return true;

    return false;
}

static void thermal_learn_step(float Tint_prev,
                               float Tint_now,
                               float Text_now,
                               float u,
                               float dt)
{
    // ============================================================
    // 0) Raccourcis vers le runtime (source unique de vérité)
    // ============================================================
    float Ta_prev = Tint_prev;
    float Ta_now = Tint_now;
    float Tm_prev = g_thermal_runtime.Tm;

    float Ra = g_thermal_runtime.Ra;
    float Rm = g_thermal_runtime.Rm;
    float Ca = g_thermal_runtime.Ca;
    float Cm = g_thermal_runtime.Cm;
    float P = g_thermal_runtime.P;

    // ============================================================
    // 1) Mesures instantanées
    // ============================================================
    float dT_meas = Ta_now - Ta_prev;
    float dTdt = dT_meas / dt;

    g_thermal_runtime.last_dT = dT_meas;
    g_thermal_runtime.last_dTdt = dTdt;

    // ============================================================
    // 2) Termes du modèle 2R2C
    // ============================================================
    float term_pertes = (Text_now - Ta_prev) / (Ra * Ca);
    float term_murs = (Tm_prev - Ta_prev) / (Rm * Ca);

    // ============================================================
    // 3) Apprentissage de P (chauffage ON)
    // ============================================================
    if (u > 0.5f)
    {
        // Formule 2R2C : P_est = (dTa/dt - pertes - murs) * Ca
        float P_est = (dTdt - term_pertes - term_murs) * Ca;
        g_thermal_runtime.last_P_est = P_est;

        const float alpha_P = 0.001f;
        if (P_est > 100 && P_est < 5000)
            P = (1 - alpha_P) * P + alpha_P * P_est;
    }

    // ============================================================
    // 4) Apprentissage de Ra (chauffage OFF)
    // ============================================================
    else
    {
        float denom = dTdt - term_murs;

        if (fabsf(denom) > 1e-7f)
        {
            // Formule 2R2C : Ra_est = (Text - Ta) / (Ca * (dTa/dt - murs))
            float Ra_est = (Text_now - Ta_prev) / (Ca * denom);
            g_thermal_runtime.last_Ra_est = Ra_est;

            const float alpha_R = 0.001f;
            if (Ra_est > 0.2f && Ra_est < 10.0f)
                Ra = (1 - alpha_R) * Ra + alpha_R * Ra_est;
        }
    }

    // ============================================================
    // 5) Apprentissage de Ca (toujours)
    // ============================================================
    if (fabsf(dTdt) > 0.00005f)
    {
        // Ca_est = P / (dTa/dt - pertes - murs)
        float Ca_est = P / (dTdt - term_pertes - term_murs);

        const float alpha_C = 0.0002f;
        if (Ca_est > 5000 && Ca_est < 300000)
            Ca = (1 - alpha_C) * Ca + alpha_C * Ca_est;
    }

    // ============================================================
    // 6) Apprentissage de Cm (via dynamique Ta↔Tm)
    // ============================================================
    float dTm_est = (Ta_prev - Tm_prev) / (Rm * Cm);
    float Cm_est = (Ta_prev - Tm_prev) / (Rm * dTm_est + 1e-9f);

    const float alpha_Cm = 0.00005f;
    if (Cm_est > 1e6 && Cm_est < 2e7)
        Cm = (1 - alpha_Cm) * Cm + alpha_Cm * Cm_est;

    // ============================================================
    // 7) Clamps physiques (via clamp())
    // ============================================================
    Ra = clamp(Ra, 0.2f, 10.0f);
    Ca = clamp(Ca, 5000.0f, 300000.0f);
    Cm = clamp(Cm, 1e6, 2e7);
    P = clamp(P, 200.0f, 5000.0f);

    // ============================================================
    // 8) Réinjection dans le runtime
    // ============================================================
    g_thermal_runtime.Ra = Ra;
    g_thermal_runtime.Ca = Ca;
    g_thermal_runtime.Cm = Cm;
    g_thermal_runtime.P = P;
}

char *prediction_engine_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    // Toujours renvoyer "ok" tant que le modèle thermique tourne
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddBoolToObject(root, "enabled_2R2C", g_thermostat_runtime.enable_2r2c);

    const char *saison_str = "inconnu";

    switch (g_thermostat_runtime.saison)
    {
    case SAISON_HIVER:
        saison_str = "hiver";
        break;
    case SAISON_INTERSAISON:
        saison_str = "intersaison";
        break;
    case SAISON_ETE:
        saison_str = "ete";
        break;
    }
    cJSON_AddStringToObject(root, "saison", saison_str);

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
    // ============================================================
    // 0) Récupération des paramètres du modèle (runtime)
    // ============================================================
    float Ca = g_thermal_runtime.Ca;
    float Cm = g_thermal_runtime.Cm;
    float Ra = g_thermal_runtime.Ra;
    float Rm = g_thermal_runtime.Rm;

    // Sécurité : éviter divisions par zéro ou dt aberrant
    if (Ca <= 0.0f || Cm <= 0.0f || Ra <= 0.0f || Rm <= 0.0f || dt <= 0.0f)
        return;

    // ============================================================
    // 1) Construction de la matrice de transition F (Euler)
    //
    // Modèle 2R2C linéarisé :
    // dTa/dt = -(1/(Ra*Ca) + 1/(Rm*Ca)) * Ta + (1/(Rm*Ca)) * Tm + P/Ca
    // dTm/dt = (1/(Rm*Cm)) * Ta - (1/(Rm*Cm)) * Tm
    //
    // Discrétisation :
    // x(k+1) = F * x(k) + B * u
    // ============================================================
    float a11 = 1.0f - dt * (1.0f / (Ra * Ca) + 1.0f / (Rm * Ca));
    float a12 = dt * (1.0f / (Rm * Ca));
    float a21 = dt * (1.0f / (Rm * Cm));
    float a22 = 1.0f - dt * (1.0f / (Rm * Cm));

    g_thermal_runtime.ekf_F[0][0] = a11;
    g_thermal_runtime.ekf_F[0][1] = a12;
    g_thermal_runtime.ekf_F[1][0] = a21;
    g_thermal_runtime.ekf_F[1][1] = a22;

    // ============================================================
    // 2) État courant x = [Ta ; Tm]
    // ============================================================
    float Ta = g_thermal_runtime.Ta;
    float Tm = g_thermal_runtime.Tm;

    // ============================================================
    // 3) Prédiction de l'état : x_pred = F*x + B*u
    //
    // B = [dt/Ca ; 0]  (la puissance agit uniquement sur Ta)
    // ============================================================
    float B0 = dt / Ca;
    float u = Puissance_chauffage;

    float Ta_pred = a11 * Ta + a12 * Tm + B0 * u;
    float Tm_pred = a21 * Ta + a22 * Tm;

    // ============================================================
    // 4) Prédiction de la covariance : P_pred = F*P*F' + Q
    // ============================================================
    float (*P)[2] = g_thermal_runtime.ekf_P;
    float (*Q)[2] = g_thermal_runtime.ekf_Q;

    float P00 = P[0][0], P01 = P[0][1];
    float P10 = P[1][0], P11 = P[1][1];

    // F * P
    float FP00 = a11 * P00 + a12 * P10;
    float FP01 = a11 * P01 + a12 * P11;
    float FP10 = a21 * P00 + a22 * P10;
    float FP11 = a21 * P01 + a22 * P11;

    // (F*P)*F' + Q
    float Pp00 = FP00 * a11 + FP01 * a12 + Q[0][0];
    float Pp01 = FP00 * a21 + FP01 * a22 + Q[0][1];
    float Pp10 = FP10 * a11 + FP11 * a12 + Q[1][0];
    float Pp11 = FP10 * a21 + FP11 * a22 + Q[1][1];

    // ============================================================
    // 5) Observation : z = Ta_mesuree
    //    H = [1 0]  (on mesure uniquement Ta)
    // ============================================================
    float H0 = g_thermal_runtime.ekf_H[0][0]; // = 1
    float H1 = g_thermal_runtime.ekf_H[0][1]; // = 0

    float z = Ta_mesuree;
    float z_pred = H0 * Ta_pred + H1 * Tm_pred;
    float y = z - z_pred; // innovation

    // ============================================================
    // 6) Innovation covariance : S = H*P_pred*H' + R
    // ============================================================
    float R_meas = 0.05f; // bruit de mesure
    float S = Pp00 * H0 * H0 + R_meas;
    if (S < 1e-6f)
        S = 1e-6f;

    // ============================================================
    // 7) Gain de Kalman : K = P_pred * H' * S^-1
    // ============================================================
    float K0 = Pp00 / S; // correction Ta
    float K1 = Pp10 / S; // correction Tm

    g_thermal_runtime.ekf_K[0][0] = K0;
    g_thermal_runtime.ekf_K[1][0] = K1;

    // ============================================================
    // 8) Mise à jour de l'état : x_new = x_pred + K*y
    // ============================================================
    Ta = Ta_pred + K0 * y;
    Tm = Tm_pred + K1 * y;

    g_thermal_runtime.Ta = Ta;
    g_thermal_runtime.Tm = Tm;

    // ============================================================
    // 9) Mise à jour de la covariance : P_new = (I - K*H) * P_pred
    // ============================================================
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

    // ============================================================
    // 10) Mise en forme des matrices pour l’interface Web
    // ============================================================
    snprintf(g_thermal_runtime.ekf_matrix_F_str, sizeof(g_thermal_runtime.ekf_matrix_F_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             a11, a12, a21, a22);

    snprintf(g_thermal_runtime.ekf_matrix_P_str, sizeof(g_thermal_runtime.ekf_matrix_P_str),
             "[[%.4f, %.4f], [%.4f, %.4f]]",
             Pn00, Pn01, Pn10, Pn11);

    snprintf(g_thermal_runtime.ekf_matrix_K_str, sizeof(g_thermal_runtime.ekf_matrix_K_str),
             "[[%.4f], [%.4f]]", K0, K1);

    snprintf(g_thermal_runtime.ekf_matrix_Q_str, sizeof(g_thermal_runtime.ekf_matrix_Q_str),
             "[[%.5f, %.5f], [%.5f, %.5f]]",
             Q[0][0], Q[0][1], Q[1][0], Q[1][1]);

    snprintf(g_thermal_runtime.ekf_matrix_H_str, sizeof(g_thermal_runtime.ekf_matrix_H_str),
             "[[%.1f, %.1f]]", H0, H1);
}

void prediction_engine_tick(void)
{
    // ============================================================
    // 1) Récupération de la prochaine consigne programmée
    // ============================================================
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

    // ============================================================
    // 2) Lecture des entrées
    // ============================================================
    float Tint_now = g_thermostat_runtime.temperature;
    float Text_now = weather_get_forecast_temp(0);
    float u = get_relay_state() ? 1.0f : 0.0f;

    // Mise à jour de l’historique Text pour la saison
    saison_update_text(Text_now);
    // Mise à jour éventuelle de la saison (et chargement profil si changement)
    saison_update();

    // Sécurité sur Tint
    if (Tint_now < -50 || Tint_now > 80)
        return;

    int64_t now_ts = time(NULL);

    // ============================================================
    // 3) Calcul du dt pour le learn
    // ============================================================
    float dt_learn = 0.0f;
    if (s_last_ts > 0)
    {
        dt_learn = (float)(now_ts - s_last_ts);
        dt_learn = clamp(dt_learn, 0.1f, 600.0f);
    }

    // ============================================================
    // 4) Tick du modèle thermique 2R2C
    // ============================================================
    thermal_2r2c_predict(Text_now, u);
    thermal_2r2c_update(Tint_now);

    // ============================================================
    // 5) Apprentissage des paramètres (si mesure précédente valide)
    // ============================================================
    if (!isnan(s_last_Tint) && s_last_ts > 0)
    {
        if (dt_learn > 0.5f && dt_learn < 600.0f)
        {
            thermal_learn_step(s_last_Tint, Tint_now, Text_now, u, dt_learn);
        }
    }

    // Mise à jour des références
    s_last_Tint = Tint_now;
    s_last_ts = now_ts;

    // ============================================================
    // 6) Tick EKF (filtre de Kalman)
    // ============================================================
    if (s_last_ekf_ts > 0)
    {
        float dt_ekf = (float)(now_ts - s_last_ekf_ts);
        dt_ekf = clamp(dt_ekf, 0.1f, 600.0f);

        float P_chauffage = u * g_thermal_runtime.P;
        prediction_engine_update(Tint_now, P_chauffage, dt_ekf);
    }
    s_last_ekf_ts = now_ts;

    // ============================================================
    // 7) Formatage des matrices EKF pour l’interface Web
    // ============================================================
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

    // ============================================================
    // 8) Early-start : calcul du temps d’anticipation
    // ============================================================
    float consigne = g_thermostat_runtime.effective_consigne;

    if (!isnan(consigne) && Tint_now >= consigne)
    {
        g_thermal_runtime.time_to_reach = 0;
        g_thermal_runtime.start_heating_at = -1;
    }
    else
    {
        g_thermal_runtime.time_to_reach =
            thermal_2r2c_time_to_reach(consigne, Text_now);

        if (g_thermal_runtime.time_to_reach > 0 && ts > 0)
            g_thermal_runtime.start_heating_at = ts - (int64_t)g_thermal_runtime.time_to_reach;
        else
            g_thermal_runtime.start_heating_at = -1;
    }
}

void prediction_engine_init(void)
{
    // ============================================================
    // 1) État thermique initial
    // ============================================================
    g_thermal_runtime.Ta = 20.0f; // Température air initiale
    g_thermal_runtime.Tm = 19.0f; // Température murs initiale

    // ============================================================
    // 2) Paramètres thermiques initiaux (valeurs par défaut)
    // ============================================================
    g_thermal_runtime.Ra = 3.0f;      // Résistance air → extérieur
    g_thermal_runtime.Rm = 5.0f;      // Résistance air ↔ murs
    g_thermal_runtime.Ca = 120000.0f; // Capacité thermique air
    g_thermal_runtime.Cm = 5e6f;      // Capacité thermique murs
    g_thermal_runtime.P = 1500.0f;    // Puissance chauffage nominale

    // ============================================================
    // 3) Initialisation EKF
    // ============================================================

    // --- Covariance P ---
    g_thermal_runtime.ekf_P[0][0] = 1.0f; // incertitude Ta
    g_thermal_runtime.ekf_P[0][1] = 0.0f;
    g_thermal_runtime.ekf_P[1][0] = 0.0f;
    g_thermal_runtime.ekf_P[1][1] = 5.0f; // incertitude Tm

    // --- Bruit de processus Q ---
    g_thermal_runtime.ekf_Q[0][0] = 0.001f;
    g_thermal_runtime.ekf_Q[0][1] = 0.0f;
    g_thermal_runtime.ekf_Q[1][0] = 0.0f;
    g_thermal_runtime.ekf_Q[1][1] = 0.0001f;

    // --- Matrice d'observation H ---
    g_thermal_runtime.ekf_H[0][0] = 1.0f; // on mesure Ta
    g_thermal_runtime.ekf_H[0][1] = 0.0f; // pas Tm

    // --- Matrice de transition F (initialisée à identité) ---
    g_thermal_runtime.ekf_F[0][0] = 1.0f;
    g_thermal_runtime.ekf_F[0][1] = 0.0f;
    g_thermal_runtime.ekf_F[1][0] = 0.0f;
    g_thermal_runtime.ekf_F[1][1] = 1.0f;

    // --- Gain de Kalman K (initialisé à zéro) ---
    g_thermal_runtime.ekf_K[0][0] = 0.0f;
    g_thermal_runtime.ekf_K[1][0] = 0.0f;

    // ============================================================
    // 4) Chauffe prédictive
    // ============================================================
    g_thermal_runtime.time_to_reach = -1;
    g_thermal_runtime.start_heating_at = -1;

    // ============================================================
    // 5) Chaînes JSON (vides au départ)
    // ============================================================
    snprintf(g_thermal_runtime.ekf_matrix_F_str, sizeof(g_thermal_runtime.ekf_matrix_F_str), "[[1,0],[0,1]]");
    snprintf(g_thermal_runtime.ekf_matrix_P_str, sizeof(g_thermal_runtime.ekf_matrix_P_str), "[[1,0],[0,5]]");
    snprintf(g_thermal_runtime.ekf_matrix_K_str, sizeof(g_thermal_runtime.ekf_matrix_K_str), "[[0],[0]]");
    snprintf(g_thermal_runtime.ekf_matrix_Q_str, sizeof(g_thermal_runtime.ekf_matrix_Q_str), "[[0.001,0],[0,0.0001]]");
    snprintf(g_thermal_runtime.ekf_matrix_H_str, sizeof(g_thermal_runtime.ekf_matrix_H_str), "[[1,0]]");
}
