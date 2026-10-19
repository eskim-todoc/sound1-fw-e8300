#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <isd_interface.h>
#include <cfx_cm3_sharedMemory.h>
#include <tdc_ut_state_cm3.h>

static ST__CFX_CM3_SharedMemory_ALL s_tdc_ut_expected_shared;

int main(void)
{
    int actual[32], expected[32];
    unsigned count;
    for (int pass = 0; pass < 2; ++pass)
    {
        tdc_ut_cm3_isd_state_t initial, original, after;
        memset(&initial, 0, sizeof(initial));
        initial.state.isd_controlState = pass ? en__isdStatus_FPGA_Ok : en__isdStatus_stimul_10V_Ok;
        initial.state.conneded_ISD = pass == 0;
        initial.changed = pass == 0;
        initial.i2c_free = pass != 0;
        original = initial;
        tdc_ut_cm3_isd_state_exchange(&original);
        memset(&cfx_cm3_sharedMemoryAll, pass ? 0x5A : 0xA5, sizeof(cfx_cm3_sharedMemoryAll));
        cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0;
        memcpy(&s_tdc_ut_expected_shared, &cfx_cm3_sharedMemoryAll, sizeof(s_tdc_ut_expected_shared));
        for (unsigned i = 0; i < 32; ++i) actual[i] = expected[i] = 123456;
        assert(tdc_ut_cm3_state_run(actual, expected, 29, &count) == -1 && count == 0);
        for (unsigned i = 0; i < 32; ++i) assert(actual[i] == 123456 && expected[i] == 123456);
        assert(tdc_ut_cm3_state_run(NULL, expected, 32, &count) == -1);
        assert(tdc_ut_cm3_state_run(actual, actual, 32, &count) == -1);
        assert(tdc_ut_cm3_state_run(actual, expected, 32, NULL) == -1);
        for (unsigned i=0;i<TDC_UT_CM3_STATE_WORDS;++i)
        {
            assert(tdc_ut_cm3_state_run(actual,expected,32,(unsigned *)(actual+i))==-1);
            assert(tdc_ut_cm3_state_run(actual,expected,32,(unsigned *)(expected+i))==-1);
            assert(tdc_ut_cm3_state_run(NULL,expected,32,(unsigned *)(expected+i))==-1);
            assert(tdc_ut_cm3_state_run(actual,NULL,32,(unsigned *)(actual+i))==-1);
            for (unsigned j=0;j<32;++j) assert(actual[j]==123456 && expected[j]==123456);
            assert(!memcmp(&cfx_cm3_sharedMemoryAll,&s_tdc_ut_expected_shared,sizeof(s_tdc_ut_expected_shared)));
        }
        cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 1;
        assert(tdc_ut_cm3_state_run(actual, expected, 32, &count) == -3 && count == 0);
        cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0;
        assert(tdc_ut_cm3_state_run(actual, expected, 30, &count) == 0 && count == 30);
        unsigned mismatches = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            if (actual[i] != expected[i])
            {
                ++mismatches;
                /* 기존 same-A 변경 플래그 불일치만 허용하는 재현 검사, 제품 합격 아님. */
                assert(i == 6 && actual[i] == 1 && expected[i] == 0);
            }
        }
        assert(mismatches == 1);
        assert(actual[30] == 123456 && actual[31] == 123456);
        assert(expected[30] == 123456 && expected[31] == 123456);
        assert(memcmp(&cfx_cm3_sharedMemoryAll, &s_tdc_ut_expected_shared, sizeof(s_tdc_ut_expected_shared)) == 0);
        after = initial;
        tdc_ut_cm3_isd_state_exchange(&after);
        assert(memcmp(&after, &initial, sizeof(initial)) == 0);
        tdc_ut_cm3_isd_state_exchange(&original);
        printf("CM3-30 pass%d: same-A FLAG A1/E0 preserved; B/clear/restore OK; 30 words\n", pass + 1);
    }
    puts("HARNESS_OK: 2 nondefault passes; known criterion flag mismatch, not whole-unit PASS");
    return 0;
}
