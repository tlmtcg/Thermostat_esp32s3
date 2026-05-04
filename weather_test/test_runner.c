#include "unity.h"

// Déclarations des fonctions de test définies dans test_weather_description.c
void test_weather_clear(void);
void test_weather_fog(void);
void test_weather_drizzle(void);
void test_weather_rain(void);
void test_weather_snow(void);
void test_weather_showers(void);
void test_weather_thunderstorm(void);
void test_weather_unknown(void);

void app_main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_weather_clear);
    RUN_TEST(test_weather_fog);
    RUN_TEST(test_weather_drizzle);
    RUN_TEST(test_weather_rain);
    RUN_TEST(test_weather_snow);
    RUN_TEST(test_weather_showers);
    RUN_TEST(test_weather_thunderstorm);
    RUN_TEST(test_weather_unknown);

    UNITY_END();
}
