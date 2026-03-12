#include "test_harness.h"
#include "nextion.h"
#include <string.h>

extern const uint8_t *mock_uart_get_tx(int *len);
extern void mock_uart_reset(void);
extern void mock_reset_all(void);

static void setup(void)
{
    mock_reset_all();
    nextion_init();
    mock_uart_reset(); /* clear init traffic */
}

void test_nextion_send_cmd_format(void)
{
    setup();
    nextion_send_cmd("page 0");

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    /* "page 0" (6) + 0xFF 0xFF 0xFF (3) = 9 bytes */
    ASSERT_EQ_INT(len, 9);
    ASSERT_TRUE(memcmp(buf, "page 0", 6) == 0);
    ASSERT_EQ_INT(buf[6], 0xFF);
    ASSERT_EQ_INT(buf[7], 0xFF);
    ASSERT_EQ_INT(buf[8], 0xFF);
}

void test_nextion_set_value_format(void)
{
    setup();
    nextion_set_value("jBar", 75);

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    /* "jBar.val=75" (11) + 0xFF×3 = 14 */
    ASSERT_EQ_INT(len, 14);
    ASSERT_TRUE(memcmp(buf, "jBar.val=75", 11) == 0);
    ASSERT_EQ_INT(buf[11], 0xFF);
}

void test_nextion_set_text_format(void)
{
    setup();
    nextion_set_text("t0", "Hello");

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    /* t0.txt="Hello" → 14 chars + 3 terminators = 17 */
    const char *expected = "t0.txt=\"Hello\"";
    int exp_len = (int)strlen(expected);
    ASSERT_EQ_INT(len, exp_len + 3);
    ASSERT_TRUE(memcmp(buf, expected, exp_len) == 0);
    ASSERT_EQ_INT(buf[exp_len], 0xFF);
}

void test_nextion_set_page(void)
{
    setup();
    nextion_set_page(2);

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    ASSERT_EQ_INT(len, 9); /* "page 2" + 0xFF×3 */
    ASSERT_TRUE(memcmp(buf, "page 2", 6) == 0);
}

void test_nextion_set_value_negative(void)
{
    setup();
    nextion_set_value("tS0", -123);

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    /* "tS0.val=-123" (12) + 3 = 15 */
    ASSERT_TRUE(memcmp(buf, "tS0.val=-123", 12) == 0);
    ASSERT_EQ_INT(buf[12], 0xFF);
}

void test_nextion_set_text_empty(void)
{
    setup();
    nextion_set_text("tKg", "");

    int len;
    const uint8_t *buf = mock_uart_get_tx(&len);
    const char *expected = "tKg.txt=\"\"";
    int exp_len = (int)strlen(expected);
    ASSERT_EQ_INT(len, exp_len + 3);
    ASSERT_TRUE(memcmp(buf, expected, exp_len) == 0);
}

int main(void)
{
    printf("\n=== Nextion Tests ===\n\n");

    RUN_TEST(test_nextion_send_cmd_format);
    RUN_TEST(test_nextion_set_value_format);
    RUN_TEST(test_nextion_set_text_format);
    RUN_TEST(test_nextion_set_page);
    RUN_TEST(test_nextion_set_value_negative);
    RUN_TEST(test_nextion_set_text_empty);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
