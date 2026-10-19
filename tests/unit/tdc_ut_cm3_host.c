#if defined(TDC_UNIT_TEST) && defined(TDC_UT_CM3_HOST)
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tdc_ut_vectors_cm3.h>
#include <tdc_ut_coordination_cm3.h>
#include <cfx_cm3_sharedMemory.h>
#include <error.h>
#include <stimulationParaCal.h>
#include <LedOutput.h>
#include <assert.h>
#include <tdc_link_control.h>

static void assert_link_state_restored(void)
{
    tdc_ut_link_control_state_t state = {0};
    tdc_ut_link_control_exchange(&state);
    tdc_ut_link_control_state_t observed = state;
    tdc_ut_link_control_exchange(&state);
    assert(observed.actual_level==187 && observed.level_valid);
    assert(observed.force_level==0 && !observed.infinite && observed.nop_override);
    assert(observed.connected_once && observed.nop_default==6);
    assert(observed.coin_balance==2 && observed.fail_count==3);
}


extern void tdc_ut_cm3_error_exchange(ST__ERROR_CODE *state);
extern int tdc_ut_cm3_dac_offset_exchange(int offset);
extern int tdc_ut_cm3_frame_duration_exchange(int duration);
extern int tdc_ut_cm3_last_frame_duration(void);
extern void tdc_ut_cm3_battery_exchange(int state[4]);
extern void tdc_ut_cm3_indicator_exchange(int state[3]);
extern void tdc_ut_cm3_led_exchange(int state[LED_SRC__MAX + 3]);
extern void tdc_ut_cm3_system_exchange(int state[16]);

/* Pure-only host link has no IO runner. Match its zero-default drain flag. */
volatile int g_tdc_ut_io_cm3_done;

/* Hardware boundary: RTT sink and timer only. Tested logic is linked from source. */
static unsigned legacy_rtt_calls;
int SEGGER_RTT_printf(unsigned buffer, const char *format, ...)
{
    (void)buffer; (void)format;
    ++legacy_rtt_calls;
    return 0;
}
int ci_timer_get_tick(void) { return 10000; }

/* These references remain in shared-memory setters but must never execute.
 * A persistence regression fails immediately instead of silently touching files. */
void fn_write_userSettingParameters(int isd_num)
{
    (void)isd_num;
    fputs("FORBIDDEN persistence boundary\n", stderr);
    abort();
}
void fn_copy_MappingData_toCM3(int map, int isd) {(void)map; (void)isd; abort();}
int ci_fft_read_pass_bin(int bands) {(void)bands; abort();}
int ci_fft_init_pass_bin(int bands) {(void)bands; abort();}
static const tdc_ut_cm3_map_ops_t forbidden_map_ops = {
    fn_copy_MappingData_toCM3, ci_fft_read_pass_bin, ci_fft_init_pass_bin};

static void assert_record_headers(unsigned unit)
{
    unsigned offset=0;
    while(offset<g_tdc_ut_cm3_vector_count)
    {
        assert(g_tdc_ut_cm3_vector_count-offset>=4);
        assert(g_tdc_ut_cm3_vector_actual[offset]==-999);
        assert(g_tdc_ut_cm3_vector_actual[offset+1]==(int)unit);
        int vector=g_tdc_ut_cm3_vector_actual[offset+2];
        int length=g_tdc_ut_cm3_vector_actual[offset+3];
        assert(vector>=1 && vector<=999 && length>0);
        assert((unsigned)length<=g_tdc_ut_cm3_vector_count-offset-4);
        offset+=4+(unsigned)length;
    }
    assert(offset==g_tdc_ut_cm3_vector_count);
}

static int compare_records(unsigned unit)
{
    unsigned offset = 0;
    int failed = 0;
    assert_record_headers(unit);
    while (offset < g_tdc_ut_cm3_vector_count)
    {
        assert(g_tdc_ut_cm3_vector_count - offset >= 4);
        assert(g_tdc_ut_cm3_vector_actual[offset] == -999);
        assert(g_tdc_ut_cm3_vector_actual[offset + 1] == (int)unit);
        unsigned length = (unsigned)g_tdc_ut_cm3_vector_actual[offset + 3];
        assert(length <= g_tdc_ut_cm3_vector_count - offset - 4);
        for (unsigned i = 0; i < length + 4; ++i)
        {
            unsigned index = offset + i;
            if (g_tdc_ut_cm3_vector_actual[index] != g_tdc_ut_cm3_vector_expected[index])
            {
                int allowed = unit == 6 && (index==5 || index==6 || index==7 || index==10 || index==11);
                allowed = allowed || (unit==30 && index==6);
                /* 300uA x 251us = 75300pC > 75000pC. Keep the disabled
                 * charge gate as a failed expectation, not a accepted result. */
                allowed = allowed || (unit==28 && index==121) || (unit==29 && index==173);
                if (!allowed)
                    fprintf(stderr,"UNEXPECTED unit=%u index=%u actual=%d expected=%d\n",unit,index,
                            g_tdc_ut_cm3_vector_actual[index],g_tdc_ut_cm3_vector_expected[index]);
                assert(allowed);
                assert(g_tdc_ut_cm3_vector_actual[index] == (index==7 && unit==6 ? 0 : 1));
                assert(g_tdc_ut_cm3_vector_expected[index] == (index==7 && unit==6 ? 1 : 0));
                printf("MISMATCH unit=%u index=%u actual=%d expected=%d\n", unit, index,
                        g_tdc_ut_cm3_vector_actual[index], g_tdc_ut_cm3_vector_expected[index]);
                ++failed;
            }
        }
        offset += length + 4;
    }
    return failed;
}

/* 모든 값은 재실행 가능한 원시 기록이다. 관찰에는 기대값을 붙이지 않는다. */
static void print_records(unsigned pass, unsigned unit)
{
    for (unsigned offset=0; offset<g_tdc_ut_cm3_vector_count;)
    {
        unsigned length=(unsigned)g_tdc_ut_cm3_vector_actual[offset+3];
        printf("RECORD {\"pass\":%u,\"unit\":%u,\"case\":%d,\"actual\":[",
               pass,unit,g_tdc_ut_cm3_vector_actual[offset+2]);
        for (unsigned i=0;i<length;++i)
            printf("%s%d",i ? "," : "",g_tdc_ut_cm3_vector_actual[offset+4+i]);
        printf("],\"expected\":");
        if (g_tdc_ut_cm3_vector_observed) printf("null");
        else
        {
            printf("[");
            for (unsigned i=0;i<length;++i)
                printf("%s%d",i ? "," : "",g_tdc_ut_cm3_vector_expected[offset+4+i]);
            printf("]");
        }
        printf("}\n");
        offset+=length+4;
    }
}

/* Callback restoration is checked with a non-NULL sentinel. The vector runner
 * must replace this only while it owns the stopped RAM calculation fixture. */
static int forbidden_copy_callback(int calculation_done)
{
    (void)calculation_done;
    abort();
}

static void assert_io_busy(unsigned unit);

static void assert_failure_observation(unsigned pass)
{
    ST__CFX_CM3_SharedMemory_ALL saved = cfx_cm3_sharedMemoryAll;
    ST__ERROR_CODE previous_error = readErrorCode();
    ST_STIUL_DAC_REGISTER_VALUE previous_dac = *readStimulDAC_RegisterValue();
    int previous_offset = readDAC_offsetValue();
    assert(tdc_ut_cm3_vectors_observe_calculation_failure()==TDC_UT_CM3_VECTOR_OBSERVED);
    assert(g_tdc_ut_cm3_vector_done && !g_tdc_ut_cm3_vector_overflow);
    assert(g_tdc_ut_cm3_vector_observed && tdc_ut_cm3_vectors_expected()==NULL);
    assert(g_tdc_ut_cm3_vector_count==40);
    assert_record_headers(29);
    print_records(pass,29);
    for (unsigned vector=0; vector<2; ++vector)
    {
        unsigned start=20*vector;
        int *values=&g_tdc_ut_cm3_vector_actual[start+4];
        assert(g_tdc_ut_cm3_vector_actual[start+2]==(int)(91+vector));
        /* Characterization, NOT a normative failed-call preservation policy:
         * the test callback skips local-array reads before the original copy.
         * Actual product T/C contents after this failure are undefined. */
        assert(values[0]==0 && values[1]==1 && values[2]==1);
        assert(values[3]==2 && values[4]==12);
        assert(values[5]==PcmBitStream_Mode_NopStandby);
        assert(values[6]==(int)vector && values[7]==(int)vector);
        assert(values[8]==0 && values[9]==0);
        assert(values[10]==600 && values[11]==631 && values[12]==700 && values[13]==731);
        assert(values[14]==0 && values[15]==0);
        printf("OBSERVED CM3-29 C%u ret=%d copy_attempt=%d local_read_skipped=%d done_before=%d done_after=%d; failure state policy pending\n",
               91+vector, values[0],values[1],values[2],values[6],values[7]);
    }
    assert(!memcmp(&saved,&cfx_cm3_sharedMemoryAll,sizeof(saved)));
    ST__ERROR_CODE current_error = readErrorCode();
    assert(!memcmp(&previous_error,&current_error,sizeof(current_error)));
    assert(!memcmp(&previous_dac,readStimulDAC_RegisterValue(),sizeof(previous_dac)));
    assert(readDAC_offsetValue()==previous_offset);
    assert(g_tdc_ut_cm3_calculation_before_copy==forbidden_copy_callback);
    assert(g_tdc_ut_cm3_map_ops==&forbidden_map_ops);
    int actual[TDC_UT_CM3_VECTOR_CAPACITY], expected[TDC_UT_CM3_VECTOR_CAPACITY];
    memcpy(actual,g_tdc_ut_cm3_vector_actual,sizeof(actual));
    memcpy(expected,g_tdc_ut_cm3_vector_expected,sizeof(expected));
    assert(tdc_ut_cm3_vectors_observe_calculation_failure()==TDC_UT_CM3_VECTOR_BUSY);
    assert(!memcmp(actual,g_tdc_ut_cm3_vector_actual,sizeof(actual)));
    assert(!memcmp(expected,g_tdc_ut_cm3_vector_expected,sizeof(expected)));
    tdc_ut_cm3_vectors_release();
    assert_io_busy(29);
    g_tdc_ut_io_cm3_done=1;
    assert(tdc_ut_cm3_vectors_observe_calculation_failure()==TDC_UT_CM3_VECTOR_BUSY);
    assert(!memcmp(actual,g_tdc_ut_cm3_vector_actual,sizeof(actual)));
    assert(!memcmp(expected,g_tdc_ut_cm3_vector_expected,sizeof(expected)));
    g_tdc_ut_io_cm3_done=0;
}

/* Setup: nondefault internal RAM states. Output: byte-exact exchange snapshot.
 * Restore: second exchange restores the inspected state; never use product setters. */
static void assert_internal_state(void (*exchange)(int *), const int *expected, unsigned count)
{
    int observed[LED_SRC__MAX + 16] = {0};
    assert(count <= sizeof(observed) / sizeof(observed[0]));
    exchange(observed);
    assert(!memcmp(observed, expected, count * sizeof(int)));
    exchange(observed);
    for (unsigned i = 0; i < count; ++i) assert(!observed[i]);
}

/* Setup: released pure records, IO result still awaiting drain.
 * Input: attempted unit run with IO done=1. Output: BUSY, all records immutable.
 * Restore: clear only the host IO boundary flag; no product logic is mocked. */
static void assert_io_busy(unsigned unit)
{
    int actual[TDC_UT_CM3_VECTOR_CAPACITY], expected[TDC_UT_CM3_VECTOR_CAPACITY];
    unsigned count = g_tdc_ut_cm3_vector_count;
    int overflow = g_tdc_ut_cm3_vector_overflow;
    int observed = g_tdc_ut_cm3_vector_observed;
    ST__CFX_CM3_SharedMemory_ALL shared = cfx_cm3_sharedMemoryAll;
    assert(!g_tdc_ut_cm3_vector_done && !g_tdc_ut_io_cm3_done);
    memcpy(actual, g_tdc_ut_cm3_vector_actual, sizeof(actual));
    memcpy(expected, g_tdc_ut_cm3_vector_expected, sizeof(expected));
    g_tdc_ut_io_cm3_done = 1;
    assert(tdc_ut_cm3_vectors_run_unit(unit) == TDC_UT_CM3_VECTOR_BUSY);
    assert(g_tdc_ut_io_cm3_done == 1 && !g_tdc_ut_cm3_vector_done);
    assert(count == g_tdc_ut_cm3_vector_count && overflow == g_tdc_ut_cm3_vector_overflow);
    assert(observed == g_tdc_ut_cm3_vector_observed);
    assert(!memcmp(actual, g_tdc_ut_cm3_vector_actual, sizeof(actual)));
    assert(!memcmp(expected, g_tdc_ut_cm3_vector_expected, sizeof(expected)));
    assert(!memcmp(&shared, &cfx_cm3_sharedMemoryAll, sizeof(shared)));
    g_tdc_ut_io_cm3_done = 0;
}

int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    static const unsigned units[] = {4,6,7,9,11,12,13,16,17,18,19,20,21,22,23,24,27,28,29,30,44};
    ST__CFX_CM3_SharedMemory_ALL saved;
    int failed = 0;
    const int battery_fixture[4] = {53, 0, 0, 0};
    const int indicator_fixture[3] = {1, 53, 2};
    const int system_fixture[16] = {3, 7, 11, 13, 0, 97, 101, 2, 1, 0, 1, 0, 1, 0, 1, 0};
    int led_fixture[LED_SRC__MAX + 3] = {0};
    int saved_battery[4], saved_indicator[3], saved_system[16], saved_led[LED_SRC__MAX + 3];
    memcpy(saved_battery, battery_fixture, sizeof(saved_battery));
    memcpy(saved_indicator, indicator_fixture, sizeof(saved_indicator));
    memcpy(saved_system, system_fixture, sizeof(saved_system));
    led_fixture[LED_SRC_BATTERY] = LED_ST_BATT_READY;
    led_fixture[LED_SRC__MAX] = 12345;
    led_fixture[LED_SRC__MAX + 1] = 1;
    led_fixture[LED_SRC__MAX + 2] = 1;
    memcpy(saved_led, led_fixture, sizeof(saved_led));
    tdc_ut_cm3_battery_exchange(saved_battery);
    tdc_ut_cm3_indicator_exchange(saved_indicator);
    tdc_ut_cm3_system_exchange(saved_system);
    tdc_ut_cm3_led_exchange(saved_led);
    ST__ERROR_CODE saved_error = {0};
    saved_error.dataProcessingErrorFlag = en__unusableMapData;
    tdc_ut_cm3_error_exchange(&saved_error);
    ST_STIUL_DAC_REGISTER_VALUE saved_dac = *readStimulDAC_RegisterValue();
    *readStimulDAC_RegisterValue() = (ST_STIUL_DAC_REGISTER_VALUE){3, 1, 77};
    int saved_offset = tdc_ut_cm3_dac_offset_exchange(123);
    int saved_duration = tdc_ut_cm3_frame_duration_exchange(73);
    assert(!g_tdc_ut_cm3_expected_shared_address && !g_tdc_ut_cm3_suppress_uart_flag && !g_tdc_ut_cm3_suppress_diagnostics);
    g_tdc_ut_cm3_expected_shared_address = 0x1234;
    g_tdc_ut_cm3_suppress_uart_flag = 7;
    g_tdc_ut_cm3_suppress_diagnostics = 7;
    assert(g_tdc_ut_cm3_calculation_before_copy==NULL);
    g_tdc_ut_cm3_calculation_before_copy=forbidden_copy_callback;
    const tdc_ut_cm3_map_ops_t *previous_map_ops=g_tdc_ut_cm3_map_ops;
    g_tdc_ut_cm3_map_ops=&forbidden_map_ops;
    int previous_map_flag=tdc_ut_cm3_map_flag_exchange(1);
    memset(&cfx_cm3_sharedMemoryAll, 0x35, sizeof(cfx_cm3_sharedMemoryAll));
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0;

    tdc_link_control_map_changed(6);
    assert(tdc_link_control_set_nop(19));
    tdc_ut_link_control_state_t link_seed = {187, true, 0, false, true, true, 6, 2, 3};
    tdc_ut_link_control_exchange(&link_seed);
    /* Fixture restore must preserve unrelated bytes, not just default zeros. */
    saved = cfx_cm3_sharedMemoryAll;
    int short_actual[26], short_expected[26];
    unsigned short_count=123;
    memset(short_actual,0x6a,sizeof(short_actual));
    memset(short_expected,0x6a,sizeof(short_expected));
    int old_flag=tdc_ut_cm3_map_flag_exchange(0);
    assert(tdc_ut_cm3_coordination_run(6,short_actual,short_expected,25,&short_count)==-1);
    assert(short_count==0 && !memcmp(&saved,&cfx_cm3_sharedMemoryAll,sizeof(saved)));
    assert(tdc_ut_cm3_coordination_run(6,short_actual,short_actual,26,&short_count)==-1);
    assert(short_count==0 && !memcmp(&saved,&cfx_cm3_sharedMemoryAll,sizeof(saved)));
    int alias_before[26]; memcpy(alias_before,short_actual,sizeof(alias_before));
    for (unsigned i=0;i<26;++i)
    {
        assert(tdc_ut_cm3_coordination_run(6,short_actual,short_expected,26,(unsigned *)(short_actual+i))==-1);
        assert(tdc_ut_cm3_coordination_run(6,short_actual,short_expected,26,(unsigned *)(short_expected+i))==-1);
        assert(tdc_ut_cm3_coordination_run(6,NULL,short_expected,26,(unsigned *)(short_expected+i))==-1);
        assert(tdc_ut_cm3_coordination_run(6,short_actual,NULL,26,(unsigned *)(short_actual+i))==-1);
        assert(!memcmp(alias_before,short_actual,sizeof(alias_before)));
        assert(!memcmp(alias_before,short_expected,sizeof(alias_before)));
        assert(!memcmp(&saved,&cfx_cm3_sharedMemoryAll,sizeof(saved)));
    }
    assert(tdc_ut_cm3_map_flag_exchange(old_flag)==0);
    assert(!g_tdc_ut_io_cm3_done);
    assert(tdc_ut_cm3_vectors_run_unit(5) == TDC_UT_CM3_VECTOR_UNSUPPORTED);
    assert(!g_tdc_ut_cm3_vector_done);
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 1;
    assert(tdc_ut_cm3_vectors_run_unit(9) == TDC_UT_CM3_VECTOR_UNSAFE);
    assert(tdc_ut_cm3_vectors_observe_calculation_failure()==TDC_UT_CM3_VECTOR_UNSAFE);
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0;
    for (unsigned pass = 1; pass <= 2; ++pass)
    for (unsigned i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
    {
        ST__ERROR_CODE previous_error = readErrorCode();
        ST_STIUL_DAC_REGISTER_VALUE previous_dac = *readStimulDAC_RegisterValue();
        int previous_offset = readDAC_offsetValue();
        int previous_duration = tdc_ut_cm3_last_frame_duration();
        int result = tdc_ut_cm3_vectors_run_unit(units[i]);
        assert(result == TDC_UT_CM3_VECTOR_OK || result == TDC_UT_CM3_VECTOR_PARTIAL);
        assert(g_tdc_ut_cm3_vector_done && !g_tdc_ut_cm3_vector_overflow);
        assert(!g_tdc_ut_cm3_vector_observed);
        assert(tdc_ut_cm3_vectors_expected() == g_tdc_ut_cm3_vector_expected);
        assert(!memcmp(&saved, &cfx_cm3_sharedMemoryAll, sizeof(saved)));
        assert_link_state_restored();
        ST__ERROR_CODE current_error = readErrorCode();
        assert(!memcmp(&previous_error, &current_error, sizeof(current_error)));
        assert(!memcmp(&previous_dac, readStimulDAC_RegisterValue(), sizeof(previous_dac)));
        assert(readDAC_offsetValue() == previous_offset);
        assert(tdc_ut_cm3_last_frame_duration() == previous_duration);
        assert(g_tdc_ut_cm3_expected_shared_address == 0x1234 && g_tdc_ut_cm3_suppress_uart_flag == 7);
        assert(g_tdc_ut_cm3_suppress_diagnostics == 7 && legacy_rtt_calls == 0);
        assert(g_tdc_ut_cm3_calculation_before_copy==forbidden_copy_callback);
        assert(g_tdc_ut_cm3_map_ops==&forbidden_map_ops);
        assert(tdc_ut_cm3_map_flag_exchange(1)==1);
        assert_internal_state(tdc_ut_cm3_battery_exchange, battery_fixture, 4);
        assert_internal_state(tdc_ut_cm3_indicator_exchange, indicator_fixture, 3);
        assert_internal_state(tdc_ut_cm3_system_exchange, system_fixture, 16);
        assert_internal_state(tdc_ut_cm3_led_exchange, led_fixture, LED_SRC__MAX + 3);
        unsigned count = g_tdc_ut_cm3_vector_count;
        int actual[TDC_UT_CM3_VECTOR_CAPACITY], expected[TDC_UT_CM3_VECTOR_CAPACITY];
        memcpy(actual, g_tdc_ut_cm3_vector_actual, sizeof(actual));
        memcpy(expected, g_tdc_ut_cm3_vector_expected, sizeof(expected));
        assert(tdc_ut_cm3_vectors_run_unit(units[i]) == TDC_UT_CM3_VECTOR_BUSY);
        assert(count == g_tdc_ut_cm3_vector_count);
        assert(!memcmp(actual, g_tdc_ut_cm3_vector_actual, sizeof(actual)));
        assert(!memcmp(expected, g_tdc_ut_cm3_vector_expected, sizeof(expected)));
        if (units[i] == 28) assert(count==122);
        if (units[i] == 29) assert(count==174 && TDC_UT_CM3_VECTOR_CAPACITY>=174);
        if (units[i] == 21) assert(count==108);
        if (units[i] == 22) assert(count==35);
        print_records(pass,units[i]);
        int mismatches = compare_records(units[i]);
        /* Exact known findings must remain failures in the RTT records. Harness
         * success only means these known outcomes reproduced, not product PASS. */
        int known = units[i] == 6 ? 5 : (units[i] == 30 || units[i] == 28 || units[i] == 29) ? 1 : 0;
        assert(mismatches == known);
        failed += mismatches != known;
        printf("UNIT %u pass=%u scope=%s words=%u mismatches=%d restore=PASS\n", units[i], pass,
                 result == TDC_UT_CM3_VECTOR_PARTIAL ? "partial" : "criteria-vectors", count, mismatches);
        tdc_ut_cm3_vectors_release();
        assert(!g_tdc_ut_cm3_vector_done);
        assert(!memcmp(actual, g_tdc_ut_cm3_vector_actual, sizeof(actual)));
        assert(!memcmp(expected, g_tdc_ut_cm3_vector_expected, sizeof(expected)));
        assert_io_busy(units[i]);
    }
    for (unsigned pass=1;pass<=2;++pass) assert_failure_observation(pass);
    tdc_ut_cm3_battery_exchange(saved_battery);
    tdc_ut_cm3_indicator_exchange(saved_indicator);
    tdc_ut_cm3_system_exchange(saved_system);
    tdc_ut_cm3_led_exchange(saved_led);
    tdc_ut_cm3_error_exchange(&saved_error);
    *readStimulDAC_RegisterValue() = saved_dac;
    (void)tdc_ut_cm3_dac_offset_exchange(saved_offset);
    (void)tdc_ut_cm3_frame_duration_exchange(saved_duration);
    g_tdc_ut_cm3_expected_shared_address = 0;
    g_tdc_ut_cm3_suppress_uart_flag = 0;
    g_tdc_ut_cm3_suppress_diagnostics = 0;
    g_tdc_ut_cm3_calculation_before_copy=NULL;
    g_tdc_ut_cm3_map_ops=previous_map_ops;
    (void)tdc_ut_cm3_map_flag_exchange(previous_map_flag);
    printf("IO done guard: 46 BUSY checks; vector/failure-observation records and shared state immutable\n");
    printf("HOST complete vector_units=%zu passes=2 known_findings=8/pass unexpected=%d; failure_observations=4 not product acceptance\n", sizeof(units)/sizeof(units[0]), failed);
    return failed ? 1 : 0;
}
#endif
