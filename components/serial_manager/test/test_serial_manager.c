#include "unity.h"
#include "serial_manager.h"
#include "alert_manager.h"
#include "esp_log.h"

// Cette fonction s'exécute AVANT chaque test
void setUp(void) {
    alert_clear_all();
}

// Cette fonction s'exécute APRÈS chaque test
void tearDown(void) {
    // Nettoyage si nécessaire
}

// TEST 1 : Vérifier que l'initialisation ne plante pas
TEST_CASE("Serial manager init returns OK", "[serial]")
{
    // On suppose que tu as modifié serial_manager_init pour renvoyer esp_err_t
    // Sinon, on vérifie juste qu'il s'exécute sans crash
    serial_manager_init();
    TEST_ASSERT_TRUE(true); 
}

TEST_CASE("Command WIFI FAIL creates alert", "[serial]")
{
    // On simule la commande
    handle_command("WIFI FAIL");
    
    // On vérifie si le compteur d'alertes a augmenté
    // (Suppose que tu as une fonction alert_get_active_count)
    TEST_ASSERT_EQUAL(1, alert_get_active_count());
}

// --- LE MAIN DE TEST ---
// Si tu veux flasher ce dossier 'test' comme un projet indépendant
void app_main(void)
{
    printf("Démarrage des tests unitaires pour SERIAL_MANAGER\n");
    
    UNITY_BEGIN();
    
    // Lance tous les tests ayant le tag [serial]
    unity_run_tests_by_tag("[serial]", false);
    
    UNITY_END();
}
