#include "led_db.h"
#include "alert_manager.h"

void setUp(void) {
    // Initialise tout le système
    led_db_init();
    // Idéalement, vider la pile d'alertes ici pour repartir à zéro
}



void tearDown(void) {
    // Nettoyage
}

TEST_CASE("Test de Santé : Priorité Sévérité", "[alert][health]")
{
    // 1. Au début, santé OK
    TEST_ASSERT_EQUAL(HEALTH_OK, alert_get_board_health());

    // 2. On ajoute une alerte mineure
    alert_add("Wifi Error");
    TEST_ASSERT_EQUAL(HEALTH_WARNING, alert_get_board_health());

    // 3. On ajoute une alerte majeure (doit écraser le warning en priorité)
    alert_add("Sensor Error");
    TEST_ASSERT_EQUAL(HEALTH_HARDWARE_FAIL, alert_get_board_health());

    // 4. On retire la majeure, on doit revenir en warning
    alert_remove("Sensor Error");
    TEST_ASSERT_EQUAL(HEALTH_WARNING, alert_get_board_health());
}
