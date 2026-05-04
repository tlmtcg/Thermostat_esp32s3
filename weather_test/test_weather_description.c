#include "unity.h"
#include "weather.h"

void setUp(void) {}
void tearDown(void) {}

static void check_desc(int code, const char *expected)
{
    const char *desc = get_weather_description(code);
    TEST_ASSERT_NOT_NULL(desc);
    TEST_ASSERT_EQUAL_STRING(expected, desc);
}

void test_weather_clear(void)
{
    check_desc(0, "Ciel clair ☀️");
    check_desc(1, "Principalement clair 🌤️");
    check_desc(2, "Partiellement nuageux ⛅");
    check_desc(3, "Couvert ☁️");
}

void test_weather_fog(void)
{
    check_desc(45, "Brouillard 🌫️");
    check_desc(48, "Brouillard 🌫️");
}

void test_weather_drizzle(void)
{
    check_desc(51, "Bruine 🌧️");
    check_desc(53, "Bruine 🌧️");
    check_desc(55, "Bruine 🌧️");
}

void test_weather_rain(void)
{
    check_desc(61, "Pluie 🌧️");
    check_desc(63, "Pluie 🌧️");
    check_desc(65, "Pluie 🌧️");
}

void test_weather_snow(void)
{
    check_desc(71, "Neige ❄️");
    check_desc(73, "Neige ❄️");
    check_desc(75, "Neige ❄️");
}

void test_weather_showers(void)
{
    check_desc(80, "Averses 🌦️");
    check_desc(81, "Averses 🌦️");
    check_desc(82, "Averses 🌦️");
}

void test_weather_thunderstorm(void)
{
    check_desc(95, "Orage ⛈️");
    check_desc(96, "Orage ⛈️");
    check_desc(99, "Orage ⛈️");
}

void test_weather_unknown(void)
{
    check_desc(-1, "Inconnu 🤷");
    check_desc(999, "Inconnu 🤷");
}

