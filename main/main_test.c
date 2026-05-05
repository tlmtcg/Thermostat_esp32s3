#include "unity.h"
#include "esp_log.h"

void app_main(void) {
    printf("\n=== Lancement des tests unitaires ===\n");
    
    // Unity scanne tous les TEST_CASE enregistrés dans le binaire
    unity_run_tests_by_tag("[serial]", false);
    
    printf("\n=== Tests termines ===\n");
}
