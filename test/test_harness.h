#pragma once

#include <stdio.h>
#include <math.h>

static int _tests_run = 0;
static int _tests_passed = 0;
static int _tests_failed = 0;
static int _current_failed = 0;

#define RUN_TEST(fn) do { \
    _current_failed = 0; \
    printf("  %-55s ", #fn); \
    _tests_run++; \
    fn(); \
    if (!_current_failed) { _tests_passed++; printf("PASS\n"); } \
} while(0)

#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    _tests_failed++; _current_failed = 1; return; } } while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ_INT(a, b) do { int _a=(a), _b=(b); if (_a != _b) { \
    printf("FAIL\n    %s:%d: %d != %d\n", __FILE__, __LINE__, _a, _b); \
    _tests_failed++; _current_failed = 1; return; } } while(0)

#define ASSERT_NEAR(a, b, eps) do { float _a=(a), _b=(b); \
    if (fabsf(_a - _b) > (eps)) { \
    printf("FAIL\n    %s:%d: %.4f != %.4f (eps=%.4f)\n", __FILE__, __LINE__, _a, _b, (float)(eps)); \
    _tests_failed++; _current_failed = 1; return; } } while(0)

#define ASSERT_NULL(ptr)     ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

#define TEST_SUMMARY() do { \
    printf("\n  %d tests: %d passed, %d failed\n", _tests_run, _tests_passed, _tests_failed); \
} while(0)

#define TEST_EXIT_CODE() (_tests_failed > 0 ? 1 : 0)
