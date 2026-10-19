/* 코어 실행 허용/맵 변경 입력. 제품 초기화와 저장소는 RAM 경계로 격리한다. */
#if defined(TDC_UNIT_TEST)
#include "tdc_ut_coordination_cm3.h"
#include "tdc_ut_vectors_cm3.h"
#include <cfx_cm3_sharedMemory.h>
#include <isd_interface_stimulationStandAlone.h>
#include <string.h>

static ST__CFX_CM3_SharedMemory_ALL s_tdc_coord_saved;
static int s_tdc_coord_trace[8];
static int s_tdc_coord_copies, s_tdc_coord_reads, s_tdc_coord_inits;

/* 실제 CFX 응답/하드웨어 초기화를 대신하는 입력 경계다. 실행 허용 결과는
 * callback에서 쓰지 않는다. 정상 기동 함수의 원래 대입을 조회한다. */
static void tdc_coord_wait(void)
{
    s_tdc_coord_trace[0] = cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration;
    s_tdc_coord_trace[1] = sharedMemoryAddresError();
    cfx_cm3_sharedMemoryAll.is_CFX_started = 1;
}
static void tdc_coord_initialize(void)
{
    s_tdc_coord_trace[2] = cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration;
    g_tdc_ut_cm3_expected_shared_address = (uintptr_t)&cfx_cm3_sharedMemoryAll;
    cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand = 32;
    s_tdc_coord_trace[3] = sharedMemoryAddresError();
}
static void tdc_coord_post(void)
{
    s_tdc_coord_trace[4] = cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration;
    s_tdc_coord_trace[5] = cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand;
}
static void tdc_coord_copy(int map, int isd)
{
    ++s_tdc_coord_copies;
    s_tdc_coord_trace[6] = map;
    s_tdc_coord_trace[7] = isd;
    /* 맵 자료 입력만 설정한다. 요청/완료/재계산 플래그는 제품 함수가 쓴다. */
    cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand = 32;
}
static int tdc_coord_read_fft(int bands)
{
    ++s_tdc_coord_reads;
    return bands == 32 ? 0 : -1;
}
static int tdc_coord_init_fft(int bands)
{
    ++s_tdc_coord_inits;
    return bands == 32 ? 0 : -1;
}

int tdc_ut_cm3_coordination_run(unsigned unit, int *actual, int *expected,
                                unsigned capacity, unsigned *count)
{
    static const tdc_ut_cm3_boot_ops_t boot = {tdc_coord_wait, tdc_coord_initialize, tdc_coord_post};
    static const tdc_ut_cm3_map_ops_t map = {tdc_coord_copy, tdc_coord_read_fft, tdc_coord_init_fft};
    unsigned required = unit == 4 ? 11u : unit == 6 ? 26u : 0u;
    if (!count) return -1;
    /* count가 결과 저장 영역 안에 있으면 레코드 쓰기가 길이를 덮어쓸 수 있다.
     * 주소 차이로 검사해 주소+길이의 정수 넘침도 피한다. 겹침은 쓰기 전 거부. */
    uintptr_t a = (uintptr_t)actual, e = (uintptr_t)expected, n = (uintptr_t)count;
    uintptr_t bytes = required * sizeof(int);
    if (required &&
        ((actual && (a<=n ? n-a<bytes : a-n<sizeof(*count))) ||
         (expected && (e<=n ? n-e<bytes : e-n<sizeof(*count))))) return -1;
    *count = 0;
    if (!required || !actual || !expected || capacity < required) return -1;
    if (a <= e ? e-a < bytes : a-e < bytes) return -1;
    if (cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration) return -3;
    s_tdc_coord_saved = cfx_cm3_sharedMemoryAll;
    uintptr_t address = g_tdc_ut_cm3_expected_shared_address;
    int diagnostics = g_tdc_ut_cm3_suppress_diagnostics;
    int uart = g_tdc_ut_cm3_suppress_uart_flag;
    int recalculate = tdc_ut_cm3_map_flag_exchange(0);
    const tdc_ut_cm3_boot_ops_t *saved_boot = g_tdc_ut_cm3_boot_ops;
    const tdc_ut_cm3_map_ops_t *saved_map = g_tdc_ut_cm3_map_ops;
    g_tdc_ut_cm3_suppress_diagnostics = 1;
    g_tdc_ut_cm3_suppress_uart_flag = 1;
    memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));
    if (unit == 4)
    {
        memset(s_tdc_coord_trace, 0, sizeof(s_tdc_coord_trace));
        g_tdc_ut_cm3_expected_shared_address = 1;
        g_tdc_ut_cm3_boot_ops = &boot;
        tdc_ut_cm3_boot_sequence();
        /* 금지 요청은 공유 허용 입력을 직접 0으로 한다. 실제 CFX 조회는 별도. */
        cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0;
        const int e[11] = {-999,4,1,7,0,1,0,0,1,32,0};
        memcpy(expected, e, sizeof(e));
        memcpy(actual, e, 4*sizeof(int));
        memcpy(actual+4, s_tdc_coord_trace, 6*sizeof(int));
        actual[10] = cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration;
        *count = 11;
    }
    else
    {
        g_tdc_ut_cm3_map_ops = &map;
        for (int request=1; request<=2; ++request)
        {
            memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));
            cfx_cm3_sharedMemoryAll.systemShare.system_opMode = en__mappingMode;
            cfx_cm3_sharedMemoryAll.userSettingValue.mapNum = 1;
            cfx_cm3_sharedMemoryAll.connected_ISD_num = 1;
            cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag = 1;
            (void)tdc_ut_cm3_map_flag_exchange(0);
            s_tdc_coord_copies = s_tdc_coord_reads = s_tdc_coord_inits = 0;
            update_CM3Status_toCFX(17);
            OnOff_3V_PMIC_CM3_to_CFX(true);
            changeProgramMapNum(request);
            unsigned p = *count;
            actual[p] = expected[p] = -999;
            actual[p+1] = expected[p+1] = 6;
            actual[p+2] = expected[p+2] = request;
            actual[p+3] = expected[p+3] = 9;
            const int e[9] = {request, request==2, request==2, request==1,
                             17,1,request==2,request==2,0};
            memcpy(expected+p+4, e, sizeof(e));
            int a[9] = {readProgramMapNum(), cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange,
                        isNewMapLoadeFlag(), isMapdateLoaded_CFX(), cfx_cm3_sharedMemoryAll.CM3_status,
                        cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX,
                        s_tdc_coord_copies, s_tdc_coord_reads, s_tdc_coord_inits};
            memcpy(actual+p+4, a, sizeof(a));
            *count += 13;
        }
    }
    /* caller가 RAM/CFX/ISR을 정지한 상태라는 전제에서 private 상태까지 복원한다. */
    cfx_cm3_sharedMemoryAll = s_tdc_coord_saved;
    g_tdc_ut_cm3_expected_shared_address = address;
    g_tdc_ut_cm3_suppress_diagnostics = diagnostics;
    g_tdc_ut_cm3_suppress_uart_flag = uart;
    (void)tdc_ut_cm3_map_flag_exchange(recalculate);
    g_tdc_ut_cm3_boot_ops = saved_boot;
    g_tdc_ut_cm3_map_ops = saved_map;
    return unit == 4 ? 1 : 0;
}
#endif
