#include "test_harness.h"
#include "weight.h"
#include "hx711.h"

/* ---- Mock hx711_get_weight (weight.c links against this) ---- */

static float mock_weights[HX711_NUM_SENSORS] = {0};
static esp_err_t mock_weight_err[HX711_NUM_SENSORS] = {0};

esp_err_t hx711_get_weight(int sensor_idx, float *weight_kg)
{
    if (sensor_idx < 0 || sensor_idx >= HX711_NUM_SENSORS || !weight_kg)
        return ESP_ERR_INVALID_ARG;
    if (mock_weight_err[sensor_idx] != ESP_OK)
        return mock_weight_err[sensor_idx];
    *weight_kg = mock_weights[sensor_idx];
    return ESP_OK;
}

static void mock_weight_reset(void)
{
    for (int i = 0; i < HX711_NUM_SENSORS; i++) {
        mock_weights[i] = 0;
        mock_weight_err[i] = ESP_OK;
    }
}

/* ---- Tests ---- */

void test_weight_init_zeroed(void)
{
    weight_init();
    weight_result_t r = weight_get_result();
    ASSERT_NEAR(r.total_kg, 0.0f, 0.01f);
    ASSERT_NEAR(r.capacity_pct, 0.0f, 0.01f);
    ASSERT_FALSE(r.stable);
    for (int i = 0; i < 4; i++) {
        ASSERT_NEAR(r.sensor_kg[i], 0.0f, 0.01f);
        ASSERT_TRUE(r.sensor_ok[i]);
    }
}

void test_weight_single_update(void)
{
    mock_weight_reset();
    weight_init();
    mock_weights[0] = 100; mock_weights[1] = 200;
    mock_weights[2] = 300; mock_weights[3] = 400;

    weight_update();
    weight_result_t r = weight_get_result();

    ASSERT_NEAR(r.total_kg, 1000.0f, 0.1f);
    ASSERT_NEAR(r.sensor_kg[0], 100.0f, 0.1f);
    ASSERT_NEAR(r.sensor_kg[1], 200.0f, 0.1f);
    ASSERT_NEAR(r.sensor_kg[2], 300.0f, 0.1f);
    ASSERT_NEAR(r.sensor_kg[3], 400.0f, 0.1f);
}

void test_weight_moving_average(void)
{
    mock_weight_reset();
    weight_init();

    /* Feed sensor 0 with values 10,20,...,100. Others stay 0. */
    for (int i = 1; i <= 10; i++) {
        mock_weights[0] = (float)(i * 10);
        weight_update();
    }

    weight_result_t r = weight_get_result();
    /* Average of 10+20+...+100 = 550/10 = 55 */
    ASSERT_NEAR(r.sensor_kg[0], 55.0f, 0.1f);
}

void test_weight_moving_average_wraps(void)
{
    mock_weight_reset();
    weight_init();

    /* First 10 updates: value = 100 */
    mock_weights[0] = 100;
    for (int i = 0; i < 10; i++) weight_update();

    /* Next 5 updates: value = 200 */
    mock_weights[0] = 200;
    for (int i = 0; i < 5; i++) weight_update();

    /* Window has 5x100 + 5x200 = 1500/10 = 150 */
    weight_result_t r = weight_get_result();
    ASSERT_NEAR(r.sensor_kg[0], 150.0f, 0.1f);
}

void test_weight_stability_not_reached(void)
{
    mock_weight_reset();
    weight_init();
    weight_set_stability(5.0f, 40);

    /* Feed same weight — but first update goes from 0→1000, breaking stability */
    mock_weights[0] = 250; mock_weights[1] = 250;
    mock_weights[2] = 250; mock_weights[3] = 250;

    /* Only 2 updates: first breaks (0→1000), second is stable */
    weight_update();
    weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_FALSE(r.stable);
}

void test_weight_stability_reached(void)
{
    mock_weight_reset();
    weight_init();
    weight_set_stability(5.0f, 3);

    mock_weights[0] = 250; mock_weights[1] = 250;
    mock_weights[2] = 250; mock_weights[3] = 250;

    /* First update: 0→1000 (resets counter). Then 4 more stable updates. */
    for (int i = 0; i < 5; i++) weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_TRUE(r.stable);
}

void test_weight_stability_resets_on_change(void)
{
    mock_weight_reset();
    weight_init();
    weight_set_stability(5.0f, 3);

    mock_weights[0] = 250; mock_weights[1] = 250;
    mock_weights[2] = 250; mock_weights[3] = 250;
    for (int i = 0; i < 5; i++) weight_update();

    /* Now a big change */
    mock_weights[0] = 500;
    weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_FALSE(r.stable);
}

void test_weight_capacity_percentage(void)
{
    mock_weight_reset();
    weight_init();
    weight_set_capacity(40000.0f);

    mock_weights[0] = 5000; mock_weights[1] = 5000;
    mock_weights[2] = 5000; mock_weights[3] = 5000;
    weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_NEAR(r.capacity_pct, 50.0f, 0.1f);
}

void test_weight_capacity_capped_at_100(void)
{
    mock_weight_reset();
    weight_init();
    weight_set_capacity(1000.0f);

    mock_weights[0] = 500; mock_weights[1] = 500;
    mock_weights[2] = 500; mock_weights[3] = 500;
    weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_NEAR(r.capacity_pct, 100.0f, 0.1f);
}

void test_weight_sensor_failure_under_threshold(void)
{
    mock_weight_reset();
    weight_init();

    mock_weight_err[2] = ESP_ERR_TIMEOUT;

    /* 9 failures: still below threshold of 10 */
    for (int i = 0; i < 9; i++) weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_TRUE(r.sensor_ok[2]);
}

void test_weight_sensor_failure_at_threshold(void)
{
    mock_weight_reset();
    weight_init();

    mock_weight_err[2] = ESP_ERR_TIMEOUT;

    for (int i = 0; i < 10; i++) weight_update();

    weight_result_t r = weight_get_result();
    ASSERT_FALSE(r.sensor_ok[2]);
    /* Other sensors still OK */
    ASSERT_TRUE(r.sensor_ok[0]);
    ASSERT_TRUE(r.sensor_ok[1]);
    ASSERT_TRUE(r.sensor_ok[3]);
}

void test_weight_sensor_recovery(void)
{
    mock_weight_reset();
    weight_init();

    mock_weight_err[2] = ESP_ERR_TIMEOUT;
    for (int i = 0; i < 10; i++) weight_update();
    ASSERT_FALSE(weight_get_result().sensor_ok[2]);

    /* Sensor comes back */
    mock_weight_err[2] = ESP_OK;
    mock_weights[2] = 100;
    weight_update();

    ASSERT_TRUE(weight_get_result().sensor_ok[2]);
}

void test_weight_holds_last_value_on_failure(void)
{
    mock_weight_reset();
    weight_init();

    mock_weights[1] = 500;
    weight_update();
    ASSERT_NEAR(weight_get_result().sensor_kg[1], 500.0f, 0.1f);

    /* Sensor fails — should hold 500 */
    mock_weight_err[1] = ESP_ERR_TIMEOUT;
    weight_update();

    /* Moving avg of [500, 500] = 500 */
    ASSERT_NEAR(weight_get_result().sensor_kg[1], 500.0f, 0.1f);
}

int main(void)
{
    printf("\n=== Weight Tests ===\n\n");

    RUN_TEST(test_weight_init_zeroed);
    RUN_TEST(test_weight_single_update);
    RUN_TEST(test_weight_moving_average);
    RUN_TEST(test_weight_moving_average_wraps);
    RUN_TEST(test_weight_stability_not_reached);
    RUN_TEST(test_weight_stability_reached);
    RUN_TEST(test_weight_stability_resets_on_change);
    RUN_TEST(test_weight_capacity_percentage);
    RUN_TEST(test_weight_capacity_capped_at_100);
    RUN_TEST(test_weight_sensor_failure_under_threshold);
    RUN_TEST(test_weight_sensor_failure_at_threshold);
    RUN_TEST(test_weight_sensor_recovery);
    RUN_TEST(test_weight_holds_last_value_on_failure);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
