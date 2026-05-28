#include "thermal_engine.h"
#include <math.h>
#include <string.h>
#include "../prediction_engine/include/prediction_engine.h"

// -----------------------------------------------------------------------------
// EKF AUGMENTÉ 2R2C
// État : [Ta, Tm, Ra, Rm, Ca, Cm, P]
// Mesure : Ta
// -----------------------------------------------------------------------------

typedef struct
{
    float Ta;
    float Tm;
    float Ra;
    float Rm;
    float Ca;
    float Cm;
    float P;
} ekf_state_t;

static float s_dt = 1.0f;

float thermal_2r2c_get_dt(void)
{
    return s_dt;
}

// Covariance 7x7
static float s_P[7][7];
// Bruit modèle 7x7
static float s_Q[7][7];
// Bruit mesure (Ta)
static float s_R = 0.05f;

// -----------------------------------------------------------------------------
// Helpers internes
// -----------------------------------------------------------------------------

static inline float clampf(float v, float vmin, float vmax)
{
    if (v < vmin)
        return vmin;
    if (v > vmax)
        return vmax;
    return v;
}

// -----------------------------------------------------------------------------
// API PUBLIQUE
// -----------------------------------------------------------------------------

void thermal_2r2c_init(float dt_seconds, float Ta0)
{
    // Pas de temps du modèle (pour predict/update)
    s_dt = dt_seconds > 0.0f ? dt_seconds : 1.0f;

    // État thermique initial
    g_thermal_runtime.Ta = Ta0;
    g_thermal_runtime.Tm = Ta0;

    // -----------------------------
    // PARAMÈTRES THERMIQUES RÉALISTES
    // -----------------------------
    g_thermal_runtime.Ra = 3.0f;         // pertes air -> extérieur
    g_thermal_runtime.Rm = 5.0f;         // échange air -> murs
    g_thermal_runtime.Ca = 120000.0f;    // capacité thermique air + mobilier
    g_thermal_runtime.Cm = 5e6f;         // inertie murs
    g_thermal_runtime.P  = 1500.0f;      // puissance radiateur

    // Dernières estimations du learn
    g_thermal_runtime.last_dT    = 0.0f;
    g_thermal_runtime.last_dTdt  = 0.0f;
    g_thermal_runtime.last_Ra_est = 0.0f;
    g_thermal_runtime.last_P_est  = 0.0f;

    // Matrices EKF (déjà initialisées ailleurs)
    // Rien à faire ici
}

void thermal_2r2c_set_params(const thermal_params_t *p)
{
    if (!p)
        return;

    // Mise à jour des paramètres réellement utilisés par le modèle
    g_thermal_runtime.Ra = p->Ra;
    g_thermal_runtime.Rm = p->Rm;
    g_thermal_runtime.Ca = p->Ca;
    g_thermal_runtime.Cm = p->Cm;
    g_thermal_runtime.P  = p->P;
}

void thermal_2r2c_get_state(thermal_state_t *s)
{
    if (!s)
        return;
    s->Ta = g_thermal_runtime.Ta;
    s->Tm = g_thermal_runtime.Tm;
}

void thermal_2r2c_get_params(thermal_params_t *p)
{
    if (!p)
        return;
    p->Ra = g_thermal_runtime.Ra;
    p->Rm = g_thermal_runtime.Rm;
    p->Ca = g_thermal_runtime.Ca;
    p->Cm = g_thermal_runtime.Cm;
    p->P  = g_thermal_runtime.P;
}

// -----------------------------------------------------------------------------
// Prédiction : x_{k+1|k} = f(x_k, u_k)
// Text : température extérieure
// u    : commande chauffage (0..1)
// -----------------------------------------------------------------------------

void thermal_2r2c_predict(float Text, float u)
{
    // Sécurité paramètres
    if (g_thermal_runtime.Ra <= 0.0f || g_thermal_runtime.Rm <= 0.0f ||
        g_thermal_runtime.Ca <= 0.0f || g_thermal_runtime.Cm <= 0.0f)
    {
        return;
    }

    float Ta = g_thermal_runtime.Ta;
    float Tm = g_thermal_runtime.Tm;

    // Modèle continu 2R2C
    float dTa = (Tm - Ta) / (g_thermal_runtime.Rm * g_thermal_runtime.Ca)
              + (Text - Ta) / (g_thermal_runtime.Ra * g_thermal_runtime.Ca)
              + (g_thermal_runtime.P * u) / g_thermal_runtime.Ca;

    float dTm = (Ta - Tm) / (g_thermal_runtime.Rm * g_thermal_runtime.Cm);

    // Intégration Euler
    g_thermal_runtime.Ta += s_dt * dTa;
    g_thermal_runtime.Tm += s_dt * dTm;

    // Clamp léger
    g_thermal_runtime.Ta = clampf(g_thermal_runtime.Ta, -40.0f, 80.0f);
    g_thermal_runtime.Tm = clampf(g_thermal_runtime.Tm, -40.0f, 80.0f);

    // Jacobien F (2x2 utile pour l’EKF)
    float dTa_dTa = -(1.0f / (g_thermal_runtime.Rm * g_thermal_runtime.Ca))
                    - (1.0f / (g_thermal_runtime.Ra * g_thermal_runtime.Ca));

    float dTa_dTm = 1.0f / (g_thermal_runtime.Rm * g_thermal_runtime.Ca);

    float dTm_dTa = 1.0f / (g_thermal_runtime.Rm * g_thermal_runtime.Cm);
    float dTm_dTm = -1.0f / (g_thermal_runtime.Rm * g_thermal_runtime.Cm);

    g_thermal_runtime.ekf_F[0][0] = 1.0f + s_dt * dTa_dTa;
    g_thermal_runtime.ekf_F[0][1] = s_dt * dTa_dTm;
    g_thermal_runtime.ekf_F[1][0] = s_dt * dTm_dTa;
    g_thermal_runtime.ekf_F[1][1] = 1.0f + s_dt * dTm_dTm;
}

// -----------------------------------------------------------------------------
// Mise à jour : y = Ta_mesurée
// -----------------------------------------------------------------------------

void thermal_2r2c_update(float Ta_measured)
{
    if (isnan(Ta_measured))
        return;

    // Innovation
    float y = Ta_measured;
    float y_hat = g_thermal_runtime.Ta;
    float e = y - y_hat;

    // S = P[0][0] + R
    float S = s_P[0][0] + s_R;
    if (S <= 0.0f)
        return;

    // Gain de Kalman
    float K[7];
    for (int i = 0; i < 7; ++i)
        K[i] = s_P[i][0] / S;

    // Mise à jour état
    g_thermal_runtime.Ta += K[0] * e;
    g_thermal_runtime.Tm += K[1] * e;
    g_thermal_runtime.Ra += K[2] * e;
    g_thermal_runtime.Rm += K[3] * e;
    g_thermal_runtime.Ca += K[4] * e;
    g_thermal_runtime.Cm += K[5] * e;
    g_thermal_runtime.P  += K[6] * e;

    // Clamps physiques
    g_thermal_runtime.Ra = clampf(g_thermal_runtime.Ra, 0.01f, 5.0f);
    g_thermal_runtime.Rm = clampf(g_thermal_runtime.Rm, 0.01f, 20.0f);
    g_thermal_runtime.Ca = clampf(g_thermal_runtime.Ca, 20000.0f, 300000.0f);
    g_thermal_runtime.Cm = clampf(g_thermal_runtime.Cm, 1e5f, 1e8f);
    g_thermal_runtime.P  = clampf(g_thermal_runtime.P, 500.0f, 5000.0f);

    // Mise à jour covariance : P = (I - K H) P
    float newP[7][7];
    for (int i = 0; i < 7; ++i)
    {
        for (int j = 0; j < 7; ++j)
        {
            newP[i][j] = s_P[i][j] - K[i] * s_P[0][j];
        }
    }

    memcpy(s_P, newP, sizeof(s_P));
}

float thermal_2r2c_time_to_reach(float Ta_target, float Text)
{
    // Copie locale de l’état EKF (on ne modifie rien)
    float Ta = g_thermal_runtime.Ta;
    float Tm = g_thermal_runtime.Tm; 
    float Ra = g_thermal_runtime.Ra;
    float Rm = g_thermal_runtime.Rm;
    float Ca = g_thermal_runtime.Ca;
    float Cm = g_thermal_runtime.Cm;
    float P  = g_thermal_runtime.P;

    // Sécurité paramètres
    if (Ra <= 0 || Rm <= 0 || Ca <= 0 || Cm <= 0 || P <= 0)
        return -1.0f;

    // Déjà au-dessus de la consigne → pas besoin de chauffer
    if (Ta >= Ta_target)
        return -1.0f;

    const float dt = 1.0f;          // 1 seconde
    const float TMAX = 6 * 3600.0f; // limite 6h
    const float u = 1.0f;           // chauffage ON

    float t = 0.0f;

    while (t < TMAX)
    {
        // Modèle 2R2C
        float dTa = ((Text - Ta) / (Ra * Ca)) + ((Tm - Ta) / (Rm * Ca)) + (P * u / Ca);

        float dTm = ((Ta - Tm) / (Rm * Cm));

        Ta += dTa * dt;
        Tm += dTm * dt;

        t += dt;

        // Condition d’atteinte
        if (Ta >= Ta_target)
            return t;
    }

    // Impossible dans les 6h
    return -1.0f;
}
