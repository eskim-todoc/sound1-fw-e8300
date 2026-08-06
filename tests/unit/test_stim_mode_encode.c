// 자극 모드 -> 내부기 레지스터 비트 변환 테스트.
//
// 2026-08-06 이전에는 이 변환이 3파일 5쌍(스위치 10블록)에 복제돼 있었다.
// isd/tdc_isd_stim_mode_encode.c 로 모으면서 이 파일이 경계값을 고정한다.
//
// 이 테스트가 isd/ 로직에 처음 붙는 호스트 테스트다. 대상이 순수 함수라
// 공유메모리 · I2C · FPGA 스텁 없이 단독으로 링크된다.

#include <tdc_isd_stim_mode_encode.h>

#include "tdc_test.h"

int main(void)
{
    // ------------------------------------------------------------------
    TEST_GROUP("기준전극 모드 (비트 [3:2]) - 모노폴라는 열거형 값 그대로");

    CHECK_EQ("monopolr_body -> 1", tdc_isd_stim_mode_reference_bits(en__monopolr_body), en__monopolr_body);
    CHECK_EQ("monopolr_rod -> 2", tdc_isd_stim_mode_reference_bits(en__monopolr_rod), en__monopolr_rod);
    CHECK_EQ("monopolr_BothRodBody -> 3", tdc_isd_stim_mode_reference_bits(en__monopolr_BothRodBody), en__monopolr_BothRodBody);

    // ------------------------------------------------------------------
    TEST_GROUP("기준전극 모드 - 기준전극을 안 쓰는 모드는 0 (접지 끊김)");

    CHECK_EQ("bipolar -> 0", tdc_isd_stim_mode_reference_bits(en__bipolar), en__referenceNA);
    CHECK_EQ("commonground -> 0", tdc_isd_stim_mode_reference_bits(en__commonground), en__referenceNA);
    CHECK_EQ("semi_simultaneously -> 0", tdc_isd_stim_mode_reference_bits(en__semi_simultaneously), en__referenceNA);
    CHECK_EQ("referenceNA 자신 -> 0", tdc_isd_stim_mode_reference_bits(en__referenceNA), en__referenceNA);

    // ------------------------------------------------------------------
    TEST_GROUP("기준전극 모드 - 2비트를 넘지 않는다");

    CHECK("body 가 2비트 안", (tdc_isd_stim_mode_reference_bits(en__monopolr_body) & ~0x3) == 0);
    CHECK("rod 가 2비트 안", (tdc_isd_stim_mode_reference_bits(en__monopolr_rod) & ~0x3) == 0);
    CHECK("BothRodBody 가 2비트 안", (tdc_isd_stim_mode_reference_bits(en__monopolr_BothRodBody) & ~0x3) == 0);
    CHECK("bipolar 가 2비트 안", (tdc_isd_stim_mode_reference_bits(en__bipolar) & ~0x3) == 0);

    // ------------------------------------------------------------------
    TEST_GROUP("자극 출력 모드 (비트 [1:0])");

    CHECK_EQ("monopolr_body -> 0", tdc_isd_stim_mode_output_bits(en__monopolr_body), 0);
    CHECK_EQ("monopolr_rod -> 0", tdc_isd_stim_mode_output_bits(en__monopolr_rod), 0);
    CHECK_EQ("monopolr_BothRodBody -> 0", tdc_isd_stim_mode_output_bits(en__monopolr_BothRodBody), 0);
    CHECK_EQ("bipolar -> 1", tdc_isd_stim_mode_output_bits(en__bipolar), 1);
    CHECK_EQ("commonground -> 2", tdc_isd_stim_mode_output_bits(en__commonground), 2);
    CHECK_EQ("semi_simultaneously -> 3", tdc_isd_stim_mode_output_bits(en__semi_simultaneously), 3);

    // ------------------------------------------------------------------
    TEST_GROUP("자극 출력 모드 - 2비트를 넘지 않는다");

    CHECK("bipolar 가 2비트 안", (tdc_isd_stim_mode_output_bits(en__bipolar) & ~0x3) == 0);
    CHECK("commonground 가 2비트 안", (tdc_isd_stim_mode_output_bits(en__commonground) & ~0x3) == 0);
    CHECK("semi_simultaneously 가 2비트 안", (tdc_isd_stim_mode_output_bits(en__semi_simultaneously) & ~0x3) == 0);

    // ------------------------------------------------------------------
    TEST_GROUP("범위 밖 값 - 원본 동작을 보존한다");

    // 원본 5쌍의 출력모드 스위치에는 default 가 없었다. 범위 밖 값이 오면
    // 아무것도 OR 되지 않아 하위 2비트가 0으로 남았고, 결과적으로 모노폴라처럼
    // 동작했다. 통합하면서 그 동작을 바꾸지 않았다는 것을 여기서 고정한다.
    CHECK_EQ("referenceNA(0) 출력모드 -> 0", tdc_isd_stim_mode_output_bits(en__referenceNA), 0);
    CHECK_EQ("범위 밖 7 출력모드 -> 0", tdc_isd_stim_mode_output_bits((EN___STIMULATION_MODE) 7), 0);
    CHECK_EQ("범위 밖 99 출력모드 -> 0", tdc_isd_stim_mode_output_bits((EN___STIMULATION_MODE) 99), 0);

    // 기준전극 쪽은 원본에 default 가 있었고 en__referenceNA 를 OR 했다.
    CHECK_EQ("범위 밖 7 기준전극 -> 0", tdc_isd_stim_mode_reference_bits((EN___STIMULATION_MODE) 7), en__referenceNA);
    CHECK_EQ("범위 밖 99 기준전극 -> 0", tdc_isd_stim_mode_reference_bits((EN___STIMULATION_MODE) 99), en__referenceNA);

    // ------------------------------------------------------------------
    TEST_GROUP("두 필드를 합친 레지스터 값 (호출부 조립 형태)");

    // 호출부는 << 2 로 자리를 만들고 OR 한다. 6개 모드의 하위 4비트를 고정한다.
    // 기대값 = (기준전극 << 2) | 출력모드
    {
        int v;

        v = (tdc_isd_stim_mode_reference_bits(en__monopolr_body) << 2) | tdc_isd_stim_mode_output_bits(en__monopolr_body);
        CHECK_EQ("body        -> 0b0100 (4)", v, 4);

        v = (tdc_isd_stim_mode_reference_bits(en__monopolr_rod) << 2) | tdc_isd_stim_mode_output_bits(en__monopolr_rod);
        CHECK_EQ("rod         -> 0b1000 (8)", v, 8);

        v = (tdc_isd_stim_mode_reference_bits(en__monopolr_BothRodBody) << 2) | tdc_isd_stim_mode_output_bits(en__monopolr_BothRodBody);
        CHECK_EQ("BothRodBody -> 0b1100 (12)", v, 12);

        v = (tdc_isd_stim_mode_reference_bits(en__bipolar) << 2) | tdc_isd_stim_mode_output_bits(en__bipolar);
        CHECK_EQ("bipolar     -> 0b0001 (1)", v, 1);

        v = (tdc_isd_stim_mode_reference_bits(en__commonground) << 2) | tdc_isd_stim_mode_output_bits(en__commonground);
        CHECK_EQ("commonground-> 0b0010 (2)", v, 2);

        v = (tdc_isd_stim_mode_reference_bits(en__semi_simultaneously) << 2) | tdc_isd_stim_mode_output_bits(en__semi_simultaneously);
        CHECK_EQ("simultaneous-> 0b0011 (3)", v, 3);
    }

    TEST_SUMMARY();
}
