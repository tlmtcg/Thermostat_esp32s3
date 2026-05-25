#include "rc_estimator.h"
#include <math.h>
#include <string.h>

static float dt;

static thermal_state_t x;   // état
static thermal_params_t prm;

static float Pk[2][2];      // covariance
static float Qk[2][2];      // bruit modèle
static float Rk;            // bruit mesure

void thermal_2r2c_set_params(const thermal_params_t *p)
{
    prm = *p;
}

void thermal_2r2c_predict(float Text, float u)
{

    if (prm.Ra <= 0 || prm.Rm <= 0 || prm.Ca <= 0 || prm.Cm <= 0)
    return;

    float Ta = x.Ta;
    float Tm = x.Tm;

    float dTa = (Tm - Ta) / prm.Rm / prm.Ca
              + (Text - Ta) / prm.Ra / prm.Ca
              + prm.P * u / prm.Ca;

    float dTm = (Ta - Tm) / prm.Rm / prm.Cm;

    // prédiction état
    x.Ta += dt * dTa;
    x.Tm += dt * dTm;

    // Jacobien F
    float F[2][2] = {
        {1 - dt*(1/prm.Rm + 1/prm.Ra)/prm.Ca, dt/(prm.Rm*prm.Ca)},
        {dt/(prm.Rm*prm.Cm), 1 - dt/(prm.Rm*prm.Cm)}
    };

    // P = F P F^T + Q
    float FP[2][2] = {0};
    float FPFt[2][2] = {0};

    for(int i=0;i<2;i++)
        for(int j=0;j<2;j++)
            for(int k=0;k<2;k++)
                FP[i][j] += F[i][k] * Pk[k][j];

    for(int i=0;i<2;i++)
        for(int j=0;j<2;j++)
            for(int k=0;k<2;k++)
                FPFt[i][j] += FP[i][k] * F[j][k];

    Pk[0][0] = FPFt[0][0] + Qk[0][0];
    Pk[1][1] = FPFt[1][1] + Qk[1][1];
}

void thermal_2r2c_update(float Ta_measured)
{
    float y = Ta_measured;
    float y_hat = x.Ta;

    float e = y - y_hat;

    float H[2] = {1, 0};

    float S = Pk[0][0] + Rk;

    float K[2] = {
        Pk[0][0] / S,
        Pk[1][0] / S
    };

    x.Ta += K[0] * e;
    x.Tm += K[1] * e;

    float I_KH[2][2] = {
        {1 - K[0], 0},
        {-K[1], 1}
    };

    float newP[2][2] = {0};

    for(int i=0;i<2;i++)
        for(int j=0;j<2;j++)
            for(int k=0;k<2;k++)
                newP[i][j] += I_KH[i][k] * Pk[k][j];

    memcpy(Pk, newP, sizeof(Pk));
}

void thermal_2r2c_get_state(thermal_state_t *s)
{
    *s = x;
}

void thermal_2r2c_get_params(thermal_params_t *p)
{
    *p = prm;
}

void thermal_2r2c_init(float dt_seconds, float Ta0)
{
    dt = dt_seconds;

    x.Ta = Ta0;
    x.Tm = Ta0;

    prm.Ra = 0.15f;
    prm.Rm = 0.10f;
    prm.Ca = 2000.0f;
    prm.Cm = 5e6f;
    prm.P  = 8000.0f;

    memset(Pk, 0, sizeof(Pk));
    Pk[0][0] = 1.0f;
    Pk[1][1] = 1.0f;

    memset(Qk, 0, sizeof(Qk));
    Qk[0][0] = 0.01f;
    Qk[1][1] = 0.01f;

    Rk = 0.05f;
}
