#include "test_harness.h"
#include "hx711.h"

extern void mock_reset_all(void);

static void setup(void)
{
    mock_reset_all();
    hx711_init();
    /* Reset scale factors (hx711_init doesn't reset these) */
    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        hx711_sensor_t *s = hx711_get_sensor(i);
        s->offset = 0;
        s->scale_factor = 1.0f;
    }
}

void test_hx711_get_sensor_valid(void)
{
    setup();
    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        ASSERT_NOT_NULL(hx711_get_sensor(i));
    }
}

void test_hx711_get_sensor_invalid(void)
{
    setup();
    ASSERT_NULL(hx711_get_sensor(-1));
    ASSERT_NULL(hx711_get_sensor(4));
    ASSERT_NULL(hx711_get_sensor(100));
}

void test_hx711_set_scale(void)
{
    setup();
    hx711_set_scale(0, 2.5f);
    hx711_sensor_t *s = hx711_get_sensor(0);
    ASSERT_NEAR(s->scale_factor, 2.5f, 0.001f);

    /* Other sensors unchanged */
    hx711_sensor_t *s1 = hx711_get_sensor(1);
    ASSERT_NEAR(s1->scale_factor, 1.0f, 0.001f);
}

void test_hx711_set_scale_invalid_idx(void)
{
    setup();
    /* Should not crash */
    hx711_set_scale(-1, 2.5f);
    hx711_set_scale(4, 2.5f);
    /* Verify valid sensors untouched */
    ASSERT_NEAR(hx711_get_sensor(0)->scale_factor, 1.0f, 0.001f);
}

void test_hx711_read_raw_invalid_args(void)
{
    setup();
    int32_t val;
    ASSERT_EQ_INT(hx711_read_raw(-1, &val), ESP_ERR_INVALID_ARG);
    ASSERT_EQ_INT(hx711_read_raw(4, &val), ESP_ERR_INVALID_ARG);
    ASSERT_EQ_INT(hx711_read_raw(0, NULL), ESP_ERR_INVALID_ARG);
}

void test_hx711_tare_invalid_args(void)
{
    setup();
    ASSERT_EQ_INT(hx711_tare(-1, 10), ESP_ERR_INVALID_ARG);
    ASSERT_EQ_INT(hx711_tare(4, 10), ESP_ERR_INVALID_ARG);
}

void test_hx711_offset_and_scale_persist(void)
{
    setup();
    hx711_sensor_t *s = hx711_get_sensor(0);
    s->offset = 5000;
    hx711_set_scale(0, 3.0f);

    /* Re-read sensor — should still have the values */
    hx711_sensor_t *s2 = hx711_get_sensor(0);
    ASSERT_EQ_INT(s2->offset, 5000);
    ASSERT_NEAR(s2->scale_factor, 3.0f, 0.001f);
}

int main(void)
{
    printf("\n=== HX711 Tests ===\n\n");

    RUN_TEST(test_hx711_get_sensor_valid);
    RUN_TEST(test_hx711_get_sensor_invalid);
    RUN_TEST(test_hx711_set_scale);
    RUN_TEST(test_hx711_set_scale_invalid_idx);
    RUN_TEST(test_hx711_read_raw_invalid_args);
    RUN_TEST(test_hx711_tare_invalid_args);
    RUN_TEST(test_hx711_offset_and_scale_persist);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
