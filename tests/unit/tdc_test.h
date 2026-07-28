#ifndef __tdc_test_h__
#define __tdc_test_h__

#include <stdio.h>

// 최소 단위 테스트 매크로.
//
// 외부 프레임워크를 쓰지 않는다. 검사 결과를 세고 실패를 종료 코드로
// 알리면 충분하며, 의존성을 늘리지 않는 편이 낫다.
//
// 카운터는 static 이다. 테스트 파일마다 독립 실행 파일을 만들고
// 러너가 순차 실행하므로 파일 간 공유가 필요 없다. 오히려 상태가
// 격리되어 한 테스트의 실패가 다른 테스트에 번지지 않는다.

static int g_test_pass;
static int g_test_fail;
static const char *g_test_group = "";

#define TEST_GROUP(name)                 \
    do {                                 \
        g_test_group = (name);           \
        printf("\n[%s]\n", g_test_group); \
    } while (0)

#define CHECK(name, cond)                          \
    do {                                           \
        if (cond) {                                \
            g_test_pass++;                         \
            printf("  PASS  %s\n", (name));        \
        } else {                                   \
            g_test_fail++;                         \
            printf("  FAIL  %s\n", (name));        \
            printf("        %s:%d\n", __FILE__, __LINE__); \
        }                                          \
    } while (0)

#define CHECK_EQ(name, actual, expected)                                  \
    do {                                                                  \
        long _a = (long) (actual);                                        \
        long _e = (long) (expected);                                      \
        if (_a == _e) {                                                   \
            g_test_pass++;                                                \
            printf("  PASS  %s\n", (name));                               \
        } else {                                                          \
            g_test_fail++;                                                \
            printf("  FAIL  %s  (기대 %ld, 실제 %ld)\n", (name), _e, _a); \
            printf("        %s:%d\n", __FILE__, __LINE__);                \
        }                                                                 \
    } while (0)

// 바이트 배열 비교. 응답 바이트열 단언에 쓴다.
#define CHECK_BYTES(name, actual, expected, len)                             \
    do {                                                                     \
        int _i, _bad = -1;                                                   \
        const unsigned char *_a = (const unsigned char *) (actual);          \
        const unsigned char *_e = (const unsigned char *) (expected);        \
        for (_i = 0; _i < (int) (len); _i++) {                               \
            if (_a[_i] != _e[_i]) { _bad = _i; break; }                      \
        }                                                                    \
        if (_bad < 0) {                                                      \
            g_test_pass++;                                                   \
            printf("  PASS  %s\n", (name));                                  \
        } else {                                                             \
            g_test_fail++;                                                   \
            printf("  FAIL  %s  (인덱스 %d: 기대 0x%02X, 실제 0x%02X)\n",    \
                   (name), _bad, _e[_bad], _a[_bad]);                        \
            printf("        %s:%d\n", __FILE__, __LINE__);                   \
        }                                                                    \
    } while (0)

#define TEST_SUMMARY()                                              \
    do {                                                            \
        printf("\n---- %d PASS / %d FAIL ----\n",                   \
               g_test_pass, g_test_fail);                           \
        return g_test_fail ? 1 : 0;                                 \
    } while (0)

#endif  // __tdc_test_h__
