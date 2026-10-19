#if defined(TDC_UNIT_TEST)
#include "tdc_ut_state_cm3.h"
#include <isd_interface.h>
#include <cfx_cm3_sharedMemory.h>
#include <stdint.h>
#include <string.h>

/* 공유 RAM 전체 복원용 고정 버퍼. 호출자는 모든 생산자를 정지해야 한다. */
static ST__CFX_CM3_SharedMemory_ALL s_tdc_ut_state_shared;
static int s_tdc_ut_state_busy;

static void record(int *actual, int *expected, unsigned *count, int case_id,
                   int state, int changed)
{
    ST__ISD_STATUS current = snd_isd_interface_get_state();
    unsigned i = *count;
    actual[i] = expected[i] = -999;
    actual[i+1] = expected[i+1] = 30;
    actual[i+2] = expected[i+2] = case_id;
    actual[i+3] = expected[i+3] = 4;
    actual[i+4] = current.isd_controlState; expected[i+4] = state;
    actual[i+5] = current.conneded_ISD; expected[i+5] = 1;
    actual[i+6] = tdc_ut_cm3_isd_state_changed(); expected[i+6] = changed;
    actual[i+7] = readCurrentPcmOutputMode(); expected[i+7] = PcmBitStream_Mode_NopStandby;
    *count = i + 8;
}

int tdc_ut_cm3_state_run(int *actual, int *expected, unsigned capacity, unsigned *count)
{
    tdc_ut_cm3_isd_state_t saved, fixture, restored;
    uintptr_t a = (uintptr_t)actual, e = (uintptr_t)expected;
    const uintptr_t bytes = TDC_UT_CM3_STATE_WORDS * sizeof(int);
    if (count == NULL) return -1;
    uintptr_t n = (uintptr_t)count;
    /* 결과 레코드가 count를 덮어쓰기 전에 겹침을 거부한다. */
    if ((actual && (a<=n ? n-a<bytes : a-n<sizeof(*count))) ||
        (expected && (e<=n ? n-e<bytes : e-n<sizeof(*count)))) return -1;
    *count = 0;
    if (actual == NULL || expected == NULL || capacity < TDC_UT_CM3_STATE_WORDS ||
        (a <= e ? e-a < bytes : a-e < bytes)) return -1;
    if (s_tdc_ut_state_busy) return -2;
    if (cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration != 0) return -3;
    s_tdc_ut_state_busy = 1;
    memcpy(&s_tdc_ut_state_shared, &cfx_cm3_sharedMemoryAll, sizeof(s_tdc_ut_state_shared));
    memset(&fixture, 0, sizeof(fixture));
    fixture.state.isd_controlState = en__isdStatus_PowerIC_Reset; /* A */
    fixture.state.conneded_ISD = true;
    fixture.i2c_free = true;
    saved = fixture;
    tdc_ut_cm3_isd_state_exchange(&saved);

    /* A 유지에는 새 진입 플래그가 없어야 한다는 기준 해석. 실제 출력은 그대로 남긴다. */
    change_isd_state(en__isdStatus_PowerIC_Reset);
    record(actual, expected, count, 1, en__isdStatus_PowerIC_Reset, 0);

    /* B 신규 진입은 앞 시험의 플래그와 독립 초기화한다. */
    restored = fixture;
    tdc_ut_cm3_isd_state_exchange(&restored);
    change_isd_state(en__isdStatus_PowerIC_OK); /* B */
    record(actual, expected, count, 2, en__isdStatus_PowerIC_OK, 1);
    clearIsdControlStateChagedFlag();
    record(actual, expected, count, 3, en__isdStatus_PowerIC_OK, 0);

    restored = saved;
    tdc_ut_cm3_isd_state_exchange(&restored);
    memcpy(&cfx_cm3_sharedMemoryAll, &s_tdc_ut_state_shared, sizeof(s_tdc_ut_state_shared));
    restored = saved;
    tdc_ut_cm3_isd_state_exchange(&restored); /* 복원된 실제 private 상태 읽기 */
    tdc_ut_cm3_isd_state_exchange(&restored); /* 교환 때문에 바뀐 내용도 원상 복구 */
    actual[24] = expected[24] = -999;
    actual[25] = expected[25] = 30;
    actual[26] = expected[26] = 99;
    actual[27] = expected[27] = 2;
    actual[28] = restored.state.isd_controlState == saved.state.isd_controlState &&
                 restored.state.conneded_ISD == saved.state.conneded_ISD &&
                 restored.changed == saved.changed && restored.i2c_free == saved.i2c_free;
    expected[28] = 1;
    actual[29] = memcmp(&cfx_cm3_sharedMemoryAll, &s_tdc_ut_state_shared, sizeof(s_tdc_ut_state_shared)) == 0;
    expected[29] = 1;
    *count = TDC_UT_CM3_STATE_WORDS;
    s_tdc_ut_state_busy = 0;
    return 0;
}
#endif
