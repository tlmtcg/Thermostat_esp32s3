#include "rc_estimator.h"
#include <math.h>
#include <string.h>

// -----------------------------------------------------------------------------
// EKF AUGMENTÉ 2R2C
// État : [Ta, Tm, Ra, Rm, Ca, Cm, P]
// Mesure : Ta
// -----------------------------------------------------------------------------

typedef struct {
    float Ta;
    float Tm;
    float Ra;
    float Rm;
    float Ca;
    float Cm;
    float P;
} ekf_state_t;

static float      s_dt = 1.0f;
static ekf_state_t s_x;

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
    if (v < vmin) return vmin;
    if (v > vmax) return vmax;
    return v;
}

// -----------------------------------------------------------------------------
// API PUBLIQUE
// -----------------------------------------------------------------------------

void thermal_2r2c_init(float dt_seconds, float Ta0)
{
    s_dt = dt_seconds > 0.0f ? dt_seconds : 1.0f;

    // État initial
    s_x.Ta = Ta0;
    s_x.Tm = Ta0;

    // Paramètres init "raisonnables" (à affiner)
    s_x.Ra = 0.15f;     // K/W
    s_x.Rm = 0.10f;     // K/W
    s_x.Ca = 2000.0f;   // J/K
    s_x.Cm = 5e6f;      // J/K
    s_x.P  = 8000.0f;   // W

    // Covariance initiale
    memset(s_P, 0, sizeof(s_P));
    for (int i = 0; i < 7; ++i) {
        s_P[i][i] = 1.0f;
    }

    // Bruit modèle : états plus bruités que paramètres
    memset(s_Q, 0, sizeof(s_Q));
    s_Q[0][0] = 0.01f;   // Ta
    s_Q[1][1] = 0.01f;   // Tm
    s_Q[2][2] = 1e-6f;   // Ra
    s_Q[3][3] = 1e-6f;   // Rm
    s_Q[4][4] = 1e-3f;   // Ca
    s_Q[5][5] = 1e-3f;   // Cm
    s_Q[6][6] = 1e-3f;   // P

    s_R = 0.05f;         // bruit mesure Ta
}

void thermal_2r2c_set_params(const thermal_params_t *p)
{
    if (!p) return;

    // On force les paramètres dans l’état EKF
    s_x.Ra = p->Ra;
    s_x.Rm = p->Rm;
    s_x.Ca = p->Ca;
    s_x.Cm = p->Cm;
    s_x.P  = p->P;
}

void thermal_2r2c_get_state(thermal_state_t *s)
{
    if (!s) return;
    s->Ta = s_x.Ta;
    s->Tm = s_x.Tm;
}

void thermal_2r2c_get_params(thermal_params_t *p)
{
    if (!p) return;
    p->Ra = s_x.Ra;
    p->Rm = s_x.Rm;
    p->Ca = s_x.Ca;
    p->Cm = s_x.Cm;
    p->P  = s_x.P;
}

// -----------------------------------------------------------------------------
// Prédiction : x_{k+1|k} = f(x_k, u_k)
// Text : température extérieure
// u    : commande chauffage (0..1)
// -----------------------------------------------------------------------------

void thermal_2r2c_predict(float Text, float u)
{
    // Sécurité paramètres
    if (s_x.Ra <= 0.0f || s_x.Rm <= 0.0f ||
        s_x.Ca <= 0.0f || s_x.Cm <= 0.0f) {
        return;
    }

    float Ta = s_x.Ta;
    float Tm = s_x.Tm;

    // Modèle continu 2R2C
    float dTa = (Tm - Ta) / (s_x.Rm * s_x.Ca)
              + (Text - Ta) / (s_x.Ra * s_x.Ca)
              + (s_x.P * u) / s_x.Ca;

    float dTm = (Ta - Tm) / (s_x.Rm * s_x.Cm);

    // Intégration Euler
    s_x.Ta += s_dt * dTa;
    s_x.Tm += s_dt * dTm;

    // Clamp léger pour éviter dérives absurdes
    s_x.Ta = clampf(s_x.Ta, -40.0f, 80.0f);
    s_x.Tm = clampf(s_x.Tm, -40.0f, 80.0f);

    // Jacobien F (7x7) – on ne dérive que par rapport à Ta/Tm,
    // les paramètres sont modélisés comme constants (dX/dt = 0)
    float F[7][7] = {0};

    // dTa/dTa
    float dTa_dTa = - (1.0f / (s_x.Rm * s_x.Ca))
                    - (1.0f / (s_x.Ra * s_x.Ca));
    // dTa/dTm
    float dTa_dTm = 1.0f / (s_x.Rm * s_x.Ca);

    // dTm/dTa
    float dTm_dTa = 1.0f / (s_x.Rm * s_x.Cm);
    // dTm/dTm
    float dTm_dTm = -1.0f / (s_x.Rm * s_x.Cm);

    // Discrétisation : F = I + dt * A
    F[0][0] = 1.0f + s_dt * dTa_dTa;
    F[0][1] =        s_dt * dTa_dTm;
    F[1][0] =        s_dt * dTm_dTa;
    F[1][1] = 1.0f + s_dt * dTm_dTm;

    // Paramètres constants → dérivée = 0 → F[i][i] = 1
    for (int i = 2; i < 7; ++i) {
        F[i][i] = 1.0f;
    }

    // P = F P F^T + Q
    float FP[7][7]   = {0};
    float FPFt[7][7] = {0};

    // FP = F * P
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 7; ++k) {
                sum += F[i][k] * s_P[k][j];
            }
            FP[i][j] = sum;
        }
    }

    // FPFt = FP * F^T
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 7; ++k) {
                sum += FP[i][k] * F[j][k]; // F^T[k][j] = F[j][k]
            }
            FPFt[i][j] = sum;
        }
    }

    // Ajout du bruit modèle
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            s_P[i][j] = FPFt[i][j] + s_Q[i][j];
        }
    }
}

// -----------------------------------------------------------------------------
// Mise à jour : y = Ta_mesurée
// -----------------------------------------------------------------------------

void thermal_2r2c_update(float Ta_measured)
{
    // Mesure invalide
    if (isnan(Ta_measured)) {
        return;
    }

    // Innovation
    float y     = Ta_measured;
    float y_hat = s_x.Ta;
    float e     = y - y_hat;

    // H = [1, 0, 0, 0, 0, 0, 0]
    // S = H P H^T + R = P[0][0] + R
    float S = s_P[0][0] + s_R;
    if (S <= 0.0f) {
        return;
    }

    // Gain de Kalman K = P H^T / S → K[i] = P[i][0] / S
    float K[7];
    for (int i = 0; i < 7; ++i) {
        K[i] = s_P[i][0] / S;
    }

    // Mise à jour état
    s_x.Ta += K[0] * e;
    s_x.Tm += K[1] * e;
    s_x.Ra += K[2] * e;
    s_x.Rm += K[3] * e;
    s_x.Ca += K[4] * e;
    s_x.Cm += K[5] * e;
    s_x.P  += K[6] * e;

    // Clamps physiques simples
    s_x.Ra = clampf(s_x.Ra, 0.01f, 5.0f);
    s_x.Rm = clampf(s_x.Rm, 0.01f, 5.0f);
    s_x.Ca = clampf(s_x.Ca, 100.0f, 20000.0f);
    s_x.Cm = clampf(s_x.Cm, 1e5f, 1e8f);
    s_x.P  = clampf(s_x.P, 1000.0f, 20000.0f);

    // Mise à jour covariance : P = (I - K H) P
    // Comme H = [1,0,0,0,0,0,0], (I - K H) a :
    // diag[0] = 1 - K[0], diag[i>0] = 1, et (i>0,0) = -K[i]
    float newP[7][7];

    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            // newP[i][j] = P[i][j] - K[i] * P[0][j]
            newP[i][j] = s_P[i][j] - K[i] * s_P[0][j];
        }
    }

    memcpy(s_P, newP, sizeof(s_P));
}

float thermal_2r2c_time_to_reach(float Ta_target, float Text)
{
    // Copie locale de l’état EKF (on ne modifie rien)
    float Ta = s_x.Ta;
    float Tm = s_x.Tm;

    float Ra = s_x.Ra;
    float Rm = s_x.Rm;
    float Ca = s_x.Ca;
    float Cm = s_x.Cm;
    float P  = s_x.P;

    // Sécurité paramètres
    if (Ra <= 0 || Rm <= 0 || Ca <= 0 || Cm <= 0 || P <= 0)
        return -1.0f;

    float t = 0.0f;
    const float dt_local = s_dt;
    const float TMAX = 6 * 3600.0f;   // limite 6h
    const float u = 1.0f;             // chauffage ON

    while (t < TMAX)
    {
        // Modèle 2R2C
        float dTa = (Tm - Ta) / (Rm * Ca)
                  + (Text - Ta) / (Ra * Ca)
                  + (P * u) / Ca;

        float dTm = (Ta - Tm) / (Rm * Cm);

        Ta += dt_local * dTa;
        Tm += dt_local * dTm;

        t += dt_local;

        if (Ta >= Ta_target)
            return t; // secondes
    }

    return -1.0f; // impossible dans les 6h
}

