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
    // État thermique estimé
    float Ta;   // Température air
    float Tm;   // Température murs

    // Chauffe prédictive
    float   time_to_reach;        // secondes
    int64_t start_heating_at;     // timestamp UNIX

    // Paramètres thermiques (runtime)
    float Ra;
    float Rm;
    float Ca;
    float Cm;
    float P;

    // Dernières estimations du learn
    float last_dT;        // Tint_now - Tint_prev
    float last_dTdt;      // (Tint_now - Tint_prev) / dt
    float last_Ra_est;    // estimation Ra (OFF)
    float last_P_est;     // estimation P (ON)

    /* =========================================================
       COMPLÉMENT : MATRICES EKF (Filtre de Kalman Étendu)
       ========================================================= */
    // Matrices numériques de l'EKF (Hypothèse : Vecteur d'état d'ordre 2 [Ta, Tm])
    float ekf_F[2][2];    // Matrice de transition d'état (Jacobienne)
    float ekf_P[2][2];    // Matrice de covariance de l'erreur d'estimation
    float ekf_Q[2][2];    // Matrice de covariance du bruit de processus
    float ekf_H[1][2];    // Matrice d'observation (lien état -> mesure de Ta)
    float ekf_K[2][1];    // Gain de Kalman

    // Représentations chaînes de caractères (Strings) pour l'export JSON / Mode Expert
    char ekf_matrix_F_str[64];
    char ekf_matrix_P_str[64];
    char ekf_matrix_Q_str[64];
    char ekf_matrix_H_str[64];
    char ekf_matrix_K_str[64];

} thermal_runtime_t;

extern thermal_runtime_t g_thermal_runtime;

void prediction_engine_init(void);

char *prediction_engine_get_json_status(void);

void prediction_engine_tick(void);

#endif

