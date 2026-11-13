/* Tests for the logger (src/nrf52_log.c), using the fake transport in mocks.c. */
#include <string.h>
#include "test_harness.h"
#include "log/nrf52_log.h"

#define LOG_LINE_MAX  159U   /* 160-byte buffer: 157 characters + "\r\n" */

static bool ends_with_crlf(int index)
{
    size_t len = mock_log_line_len(index);
    const char *line = mock_log_line(index);
    return len >= 2U && line[len - 2U] == '\r' && line[len - 1U] == '\n';
}

static void test_init_uses_tx_pin(void)
{
    CHECK(nrf52_log_init(NRF52_PIN(0, 6)) == NRF52_OK);
    CHECK(mock_log_tx_pin() == NRF52_PIN(0, 6));
}

static void test_line_format(void)
{
    mock_set_now(1234);
    NRF52_LOGI("app", "value=%d", 42);

    CHECK(mock_log_line_count() == 1);
    CHECK(strcmp(mock_log_line(0), "[1234] I app: value=42\r\n") == 0);
    CHECK(mock_log_line_len(0) == strlen("[1234] I app: value=42\r\n"));
}

static void test_one_write_per_line(void)
{
    NRF52_LOGI("a", "first");
    NRF52_LOGI("b", "second");

    CHECK(mock_log_line_count() == 2);
    CHECK(strcmp(mock_log_line(0), "[0] I a: first\r\n") == 0);
    CHECK(strcmp(mock_log_line(1), "[0] I b: second\r\n") == 0);
}

static void test_level_letters(void)
{
    nrf52_log_set_level(NRF52_LOG_DEBUG);
    NRF52_LOGE("t", "e");
    NRF52_LOGW("t", "w");
    NRF52_LOGI("t", "i");
    NRF52_LOGD("t", "d");

    CHECK(mock_log_line_count() == 4);
    CHECK(strcmp(mock_log_line(0), "[0] E t: e\r\n") == 0);
    CHECK(strcmp(mock_log_line(1), "[0] W t: w\r\n") == 0);
    CHECK(strcmp(mock_log_line(2), "[0] I t: i\r\n") == 0);
    CHECK(strcmp(mock_log_line(3), "[0] D t: d\r\n") == 0);
}

static void test_default_level_hides_debug(void)
{
    NRF52_LOGD("t", "hidden");
    NRF52_LOGI("t", "shown");

    CHECK(mock_log_line_count() == 1);
    CHECK(strcmp(mock_log_line(0), "[0] I t: shown\r\n") == 0);
}

static void test_set_level_filters(void)
{
    nrf52_log_set_level(NRF52_LOG_ERROR);
    NRF52_LOGW("t", "hidden");
    NRF52_LOGI("t", "hidden");
    NRF52_LOGE("t", "shown");

    CHECK(mock_log_line_count() == 1);
    CHECK(strcmp(mock_log_line(0), "[0] E t: shown\r\n") == 0);
}

static void test_failed_init_disables_logging(void)
{
    mock_set_log_port_status(NRF52_ERR_PARAM);
    CHECK(nrf52_log_init(NRF52_PIN(0, 6)) == NRF52_ERR_PARAM);

    NRF52_LOGE("t", "dropped");
    CHECK(mock_log_line_count() == 0);
}

static void test_reinit_after_failure(void)
{
    mock_set_log_port_status(NRF52_ERR_PARAM);
    CHECK(nrf52_log_init(0) == NRF52_ERR_PARAM);
    mock_set_log_port_status(NRF52_OK);
    CHECK(nrf52_log_init(0) == NRF52_OK);

    NRF52_LOGI("t", "back");
    CHECK(mock_log_line_count() == 1);
}

static void test_long_message_truncated(void)
{
    char big[300];
    memset(big, 'x', sizeof(big) - 1U);
    big[sizeof(big) - 1U] = '\0';

    NRF52_LOGI("t", "%s", big);

    CHECK(mock_log_line_count() == 1);
    CHECK(mock_log_line_len(0) == LOG_LINE_MAX);
    CHECK(strncmp(mock_log_line(0), "[0] I t: xxx", 12) == 0);
    CHECK(ends_with_crlf(0));
}

static void test_long_tag_truncated(void)
{
    char tag[200];
    memset(tag, 'g', sizeof(tag) - 1U);
    tag[sizeof(tag) - 1U] = '\0';

    NRF52_LOGI(tag, "message");

    CHECK(mock_log_line_count() == 1);
    CHECK(mock_log_line_len(0) == LOG_LINE_MAX);
    CHECK(strncmp(mock_log_line(0), "[0] I ggg", 9) == 0);
    CHECK(ends_with_crlf(0));
}

static void test_exact_fit_not_truncated(void)
{
    /* "[0] I t: " is 9 characters; 148 more fill the 157-character limit. */
    char body[149];
    memset(body, 'y', 148);
    body[148] = '\0';

    NRF52_LOGI("t", "%s", body);

    CHECK(mock_log_line_len(0) == LOG_LINE_MAX);
    CHECK(mock_log_line(0)[LOG_LINE_MAX - 3U] == 'y');
    CHECK(ends_with_crlf(0));
}

static void test_empty_message(void)
{
    NRF52_LOGI("t", "%s", "");

    CHECK(strcmp(mock_log_line(0), "[0] I t: \r\n") == 0);
}

static void test_timestamp_wraps_as_unsigned(void)
{
    mock_set_now(0xFFFFFFFFUL);
    NRF52_LOGI("t", "max");

    CHECK(strcmp(mock_log_line(0), "[4294967295] I t: max\r\n") == 0);
}

static void test_integer_formats(void)
{
    NRF52_LOGI("t", "%d %u %ld %lx %c", -5, 7U, -100000L, 0xBEEFUL, 'z');

    CHECK(strcmp(mock_log_line(0), "[0] I t: -5 7 -100000 beef z\r\n") == 0);
}

const test_case_t g_log_tests[] = {
    { "init_uses_tx_pin",              test_init_uses_tx_pin },
    { "line_format",                   test_line_format },
    { "one_write_per_line",            test_one_write_per_line },
    { "level_letters",                 test_level_letters },
    { "default_level_hides_debug",     test_default_level_hides_debug },
    { "set_level_filters",             test_set_level_filters },
    { "failed_init_disables_logging",  test_failed_init_disables_logging },
    { "reinit_after_failure",          test_reinit_after_failure },
    { "long_message_truncated",        test_long_message_truncated },
    { "long_tag_truncated",            test_long_tag_truncated },
    { "exact_fit_not_truncated",       test_exact_fit_not_truncated },
    { "empty_message",                 test_empty_message },
    { "timestamp_wraps_as_unsigned",   test_timestamp_wraps_as_unsigned },
    { "integer_formats",               test_integer_formats },
};
const size_t g_log_test_count = TEST_COUNT(g_log_tests);
