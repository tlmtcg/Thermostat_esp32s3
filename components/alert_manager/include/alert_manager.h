/**
 * @file alert_manager.h
 * @brief Gestion centralisée des alertes (activation, suppression, historique).
 *
 * Pipeline LED A :
 *  - Les alarmes actives sont stockées sous forme d’index vers led_db.
 *  - led_task lit la liste triée via alert_get_active_list().
 *  - Le tri dépend du mode choisi : activation ou sévérité.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ============================================================================
   TYPES
   ============================================================================ */

/**
 * @brief Événements envoyés aux callbacks enregistrés.
 */
typedef enum {
    ALERT_EVENT_ADDED,
    ALERT_EVENT_REMOVED
} alert_event_t;

/**
 * @brief Structure d’un événement d’historique.
 */
typedef struct {
    time_t timestamp;
    char name[32];
    bool activated;   // true = ajout, false = suppression
} alert_log_t;

/**
 * @brief Ordre d’affichage des alarmes.
 */
typedef enum {
    ALERT_ORDER_ACTIVATION = 0,   // ordre d’arrivée (FIFO)
    ALERT_ORDER_SEVERITY   = 1    // sévérité décroissante
} alert_order_t;

/**
 * @brief Prototype du callback utilisateur.
 */
typedef void (*alert_callback_t)(alert_event_t event, const alert_log_t *log);

/* ============================================================================
   INITIALISATION
   ============================================================================ */

/**
 * @brief Initialise le gestionnaire d’alertes.
 */
void alert_manager_init(void);

/* ============================================================================
   CALLBACK
   ============================================================================ */

/**
 * @brief Enregistre un callback appelé à chaque ajout/suppression d’alerte.
 */
void alert_register_callback(alert_callback_t cb);

/* ============================================================================
   AJOUT / SUPPRESSION D’ALERTE
   ============================================================================ */

/**
 * @brief Active une alerte (par son nom).
 * @return true si ajoutée, false sinon.
 */
bool alert_add(const char *name);

/**
 * @brief Désactive une alerte (par son nom).
 * @return true si supprimée, false sinon.
 */
bool alert_remove(const char *name);

/**
 * @brief Supprime toutes les alertes actives.
 */
void alert_clear_all(void);

/* ============================================================================
   TRI DES ALERTES
   ============================================================================ */

/**
 * @brief Définit l’ordre d’affichage des alarmes.
 */
void alert_set_order(alert_order_t order);

/**
 * @brief Renvoie l’ordre d’affichage actuel.
 */
alert_order_t alert_get_order(void);

/* ============================================================================
   LISTE DES ALERTES ACTIVES
   ============================================================================ */

/**
 * @brief Renvoie le nombre d’alertes actives.
 */
int alert_get_active_count(void);

/**
 * @brief Renvoie une liste triée d’index d’alertes actives.
 *
 * IMPORTANT :
 *  - La liste renvoyée est une COPIE triée.
 *  - Elle reste valide jusqu’au prochain appel.
 */
const int *alert_get_active_list(void);

/**
 * @brief Renvoie l’index de l’alarme la plus prioritaire.
 *        (utilisé uniquement pour compatibilité)
 */
int alert_get_top_priority(void);

/* ============================================================================
   SÉVÉRITÉ
   ============================================================================ */

/**
 * @brief Calcule la sévérité d’une alarme selon son nom.
 *        (Panne/FAIL/ERROR = 2, Attente/WAIT = 1)
 */
int get_alarm_severity(const char *name);

/* ============================================================================
   HISTORIQUE
   ============================================================================ */

/**
 * @brief Renvoie une entrée d’historique par index.
 */
const alert_log_t *alert_get_by_index(int i);

/**
 * @brief Ajoute une entrée d’historique (interne).
 */
void alert_push_history(const alert_log_t *log);

/**
 * @brief Affiche l’historique complet dans les logs.
 */
void alert_get_history(void);


/**
 * @brief Récupère la liste des alarmes actives avec leurs métadonnées.
 * @param alerts Tableau pré-alloué pour stocker les alertes.
 * @param max_alerts Taille maximale du tableau.
 * @return Nombre d'alarmes actives copiées.
 */
int alert_get_active_alerts(alert_log_t alerts[], int max_alerts);