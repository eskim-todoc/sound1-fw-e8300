/**
 * @file app.c
 * @brief Calibration sample application
 * @details
 *       Calibrates voltage regulators and the internal osciallator, and
 *       stores the trimming values to the onboard NVM.
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */
#include "app_CM3.h"

#include "SEGGER_RTT.h"
#include "cfx_cm3_sharedMemory.h"

void ci_led_init(void);
void ci_led_color(int color);
void ci_handle_result(int result, char *task_message);

#define PIN_CFG_LED (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

NVMLIB_DEVICE  nvmdevice;
NVMLIB_DEVICE *device;
uint32_t       chess_storage(IOMEM) buffer[4096]; /* buffer should be size of largest erase unit */
uint32_t       chess_storage(IOMEM) manu_table_buffer[MANU_TABLE_SIZE];
unsigned int   nvm_access;
unsigned int   success;

/** NVM library scratchpad buffer */
static uint32_t chess_storage(IOMEM) cmd_buffer[16];

#define CI_LED_COLOR_BLACK    0
#define CI_LED_COLOR_PROGRESS 1
#define CI_LED_COLOR_SUCCESS  2
#define CI_LED_COLOR_FAIL     3

static volatile int      g_ci_lsad_sample_count      = 0;
static volatile int      g_ci_lsad_sample_count_prev = 0;
static volatile uint32_t g_ci_lsad_sum[8]            = {0};

FATFS   g_ci_mount;
FIL     g_ci_ohdl;
FILINFO g_fno;

void ci_led_init(void)
{
    SYS_DIO_CONFIG(CI_LED_DIO_RED, PIN_CFG_LED);
    SYS_DIO_CONFIG(CI_LED_DIO_GREEN, PIN_CFG_LED);
    SYS_DIO_CONFIG(CI_LED_DIO_BLUE, PIN_CFG_LED);

    ci_led_color(CI_LED_COLOR_BLACK);
}

void ci_led_color(int color)
{
    switch (color)
    {
        case CI_LED_COLOR_PROGRESS:
            Sys_GPIO_Set_Low(CI_LED_DIO_RED);
            Sys_GPIO_Set_Low(CI_LED_DIO_GREEN);
            Sys_GPIO_Set_High(CI_LED_DIO_BLUE);
            break;

        case CI_LED_COLOR_SUCCESS:
            Sys_GPIO_Set_Low(CI_LED_DIO_RED);
            Sys_GPIO_Set_High(CI_LED_DIO_GREEN);
            Sys_GPIO_Set_Low(CI_LED_DIO_BLUE);
            break;

        case CI_LED_COLOR_FAIL:
            Sys_GPIO_Set_High(CI_LED_DIO_RED);
            Sys_GPIO_Set_Low(CI_LED_DIO_GREEN);
            Sys_GPIO_Set_Low(CI_LED_DIO_BLUE);
            break;

        case CI_LED_COLOR_BLACK:
        default:
            Sys_GPIO_Set_Low(CI_LED_DIO_RED);
            Sys_GPIO_Set_Low(CI_LED_DIO_GREEN);
            Sys_GPIO_Set_Low(CI_LED_DIO_BLUE);
            break;
    }
}

void ci_handle_result(int result, char *task_message)
{
    if (result == ERRNO_NO_ERROR)
    {
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_GREEN);
        SEGGER_RTT_printf(0, "PASS");
    }
    else
    {
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_RED);
        SEGGER_RTT_printf(0, "FAIL");
    }

    SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
    SEGGER_RTT_printf(0, " : %s \r\n", task_message);

    if (result != ERRNO_NO_ERROR)
    {
        ExitApp(result);
    }
}

void ci_print_target_mv(char *target_name, int mv)
{
    SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
    SEGGER_RTT_printf(0, "%s : ", target_name);
    SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_CYAN);
    SEGGER_RTT_printf(0, "%4u \r\n", mv);
    SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
}

int NVMInit_local(void)
{
    /* Configure SPI0 RET */
    int ret;
    device = &nvmdevice;

    ret = NVMLIB_Interface_Configure(device, (void *) SPI0, APP_SPI_DEFAULT_CFG, 3840U, SPI_PIN_CFG, SPI0_SCLK_PIN, SPI0_SSEL_PIN, SPI0_MOSI_PIN, SPI0_MISO_PIN);
    if (ret != ARM_DRIVER_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "NVMLIB_Interface_Configure()");
        return ret;
    }

    /* Init NVMLIB */
    ret = NVMLIB_Initialize(device, NVMLIB_STORAGE_AUTO, cmd_buffer);

    if (ret != ARM_DRIVER_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "NVMLIB_Initialize()");
    }

    return ret;
}

void ADCInit(unsigned int channel_num)
{
    // clang-format off
    /* IOC input configuration */
    D_IOC0->INPUT_CFG = ( IOC_INPUT_CFG_IN3_NONE | IOC_INPUT_CFG_IN2_NONE
    		            | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN0_NONE );

    D_IOC0->INPUT_CFG &= ~(D_IOC_INPUT_CFG_IN0_STORE_Mask
    		               << (channel_num * (D_IOC_INPUT_CFG_IN1_STORE_Pos - D_IOC_INPUT_CFG_IN0_STORE_Pos)));

    /* Set ADC sampling frequency */
    AUDIO->ADC_SAMPLE_FREQ_CFG = ADC_MODDIV_BY15;

    /* Enable and configure ADC for simulation */
    AUDIO->ADC_CFG[channel_num] = ( ADC_FBDAC_DEM_DISABLE | ADC_FBDAC_MAX_DRIVE_64
    		                      | ADC_FBDAC_MIN_DRIVE_1 | ADC_RIN_4K57
                                  | ADC_COMP_CURRENT_2UA  | ADC_CURRENT_14P5UA );

    AUDIO->ADC_CTRL[channel_num] = ( ADC_ENABLE         | ADC_COMP_ENABLE         | ADC_SEL_VREG
    		                       | ADC_FBDAC_INTERNAL | ADC_FBDAC_SRC_INT_FILT0 | ADC_MODE_MAX_1P6V
								   | ADC_AIN_RBIAS_VREG );

    AUDIO->ADC_FDEC_CTRL[channel_num] = ADC_FDEC_OPTIMAL;

    /* Enable decimation filter */
    AUDIO->ADC_DEC_CTRL[channel_num] = DEC_FILTER_CFG;
    // clang-format on
}

void IntInit(void)
{
    /* Mask, disable and clear all (pending) interrupts */
#if defined(__chess__)
    SYS_CFX_INT_MASTER_DISABLE(D_INT);
    SYS_CFX_INT_DISABLEALL(D_INT);
    SYS_CFX_INT_CLEARALLPENDING(D_INT)
#else
    __disable_irq();
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
#endif
}

void delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}

void Initialize(void)
{
    int ret    = 0;
    nvm_access = 1;
    success    = 0;

    // Initialize SEGGER RTT control block
    SEGGER_RTT_Init();

    // Configure DIOs for SWJ-DP
    Sys_DIO_CM3JTAGConfig(true, false);

    // Debug unlock
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                            // I2C
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                              // Access
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk;  // SEGGER RTT viewer

    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();

    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    /* Disable CFX */
    ST__CFX_CM3_SharedMemory_ALL *Addr_SharedMem = (ST__CFX_CM3_SharedMemory_ALL *) CFX_CM3_SHARED_MEMORY_BASE_ADDR;
    if (Addr_SharedMem->is_CFX_started == 1)
    {
        Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1;

        volatile int ms = 0;

        while (1)
        {
            if (Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX == 0)
            {
                break;
            }

            delay_ms(10);
            ms += 10;

            if (ms >= 1000)
            {
                SEGGER_RTT_printf(0, "Waiting CFX sleep. \r\n");
                ms = 0;
                break;
            }
        }
    }

    /* Disable and clear all (pending) interrupts. */
    IntInit();

    /* Configure specified DIO for GPIO input */
    ci_led_init();

    /* Set clock sources and dividers to default in case the system was previously configured. */
    ANALOG->OSC_CTRL_0 = OSC_FREQ_7M68 | OSC_FREQ_FINE_NOM;
    D_CLK->CFG_0       = SYSCLK_SEL_OSCCLK;
    D_CLK->CFG_1       = SLOWCLK_SRC_SYSCLK | SLOWCLK_PRESCALE_6;

    /* Initialize the NVM device and library */
    ret |= NVMInit_local();
    // ret |= NVMInit(NULL);

    /* Initialize calibration library NVM support variables */
    ret |= Calibrate_Support_Initialize(device, manu_table_buffer, buffer);
    if (ret != ARM_DRIVER_OK)
    {
#if 1
        ci_handle_result(ERRNO_GENERAL_FAILURE, "Calibrate_Support_Initialize()");
        nvm_access = 0;
#else
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_RED);
        SEGGER_RTT_printf(0, "FAIL");
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
        SEGGER_RTT_printf(0, " : %s \r\n", "Calibrate_Support_Initialize()");
#endif
    }

    /* Set to true if slow stabilizing voltages such as VDDOD/VMIC/VDDIF are loaded
     * using a 1k to 2k ohm resistor. Decreases power calibration time significantly. */
    Calibrate_Power_VoltageLoaded(false);

    /* Enable VMIC and VDDOD regulators since they're disabled by default.
     * A delay will be needed to let the voltages stabilize if these
     * lines are placed directly before VDDOD and VMIC calibration function calls. */
    ANALOG->VDDOD_CTRL |= VDDOD_REG_ENABLE;
    ANALOG->VMIC_CTRL |= VMIC_REG_ENABLE;
}

#define REPEAT_FUNC(func, times)                                                                                                                                                                                                                                                                                               \
    for (unsigned int i = 0; i < times; i++)                                                                                                                                                                                                                                                                                   \
    {                                                                                                                                                                                                                                                                                                                          \
        curr_result = func;                                                                                                                                                                                                                                                                                                    \
        if (curr_result == ERRNO_NO_ERROR)                                                                                                                                                                                                                                                                                     \
        {                                                                                                                                                                                                                                                                                                                      \
            break;                                                                                                                                                                                                                                                                                                             \
        }                                                                                                                                                                                                                                                                                                                      \
    }                                                                                                                                                                                                                                                                                                                          \
    result |= curr_result;                                                                                                                                                                                                                                                                                                     \
                                                                                                                                                                                                                                                                                                                               \
    ci_handle_result(result, #func);

int main(void)
{
    static volatile int break_point = 0;

    int      result      = 0;
    int      curr_result = 0;
    uint16_t vddc_ret_trim;
    uint16_t vddm_ret_trim_standby;
    uint16_t vddm_ret_trim_retention;
    int      ret;

    break_point = 1;

    /* Initialise the system including the clocks so we start from a known state */
    Initialize();

    ci_led_color(CI_LED_COLOR_PROGRESS);

    // SEGGER RTT viewer 연결 대기 (SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL)
    for (int i = 0; i < BUFFER_SIZE_UP; i++)
    {
        SEGGER_RTT_printf(0, " ");
    }

    SEGGER_RTT_printf(0, "%s", RTT_CTRL_CLEAR);
    SEGGER_RTT_printf(0, "\r\n\n");
    SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_YELLOW);
    SEGGER_RTT_printf(0, "CALIBRATION \r\n");
    SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
    SEGGER_RTT_printf(0, "\r\n");

    ci_print_target_mv(" VREG              TARGET", VREG_TARGET);
    ci_print_target_mv("VDDCM MIN CP DELTA TARGET", VDDCM_MIN_CP_DELTA_TARGET);
    ci_print_target_mv(" VDDC    RETENTION TARGET", VDDC_RETENTION_TARGET);
    ci_print_target_mv(" VDDC        LIMIT TARGET", VDDC_LIMIT_TARGET);
    ci_print_target_mv(" VDDC       ACTIVE TARGET", VDDC_ACTIVE_TARGET);
    ci_print_target_mv(" VDDM    RETENTION TARGET", VDDM_RETENTION_TARGET);
    ci_print_target_mv(" VDDM      STANDBY TARGET", VDDM_STANDBY_TARGET);
    ci_print_target_mv(" VDDM       ACTIVE TARGET", VDDM_ACTIVE_TARGET);
    ci_print_target_mv(" VDDA       ACTIVE TARGET", VDDA_ACTIVE_TARGET);
    ci_print_target_mv("VDDIF       ACTIVE TARGET", VDDIF_ACTIVE_TARGET);
    ci_print_target_mv(" VMIC       ACTIVE TARGET", VMIC_ACTIVE_TARGET);
    ci_print_target_mv("VDDOD       ACTIVE TARGET", VDDOD_ACTIVE_TARGET);
    SEGGER_RTT_printf(0, "\r\n");

    Calibrate_Clock_Initialize();
    REPEAT_FUNC(Calibrate_Clock_Internal_OSC(7680, REF_CLK_DIO), 1);

    /* Power Blocks Calibration */
    /* VREG and LSAD always need to be calibrated first.
     * Hardware Requirements:
     * - Apply 1.5 V or higher to VBAT; if VBAT is not 1.5 V the value passed to the following function needs to be
     * updated */

    // VREG and LSAD
    REPEAT_FUNC(Calibrate_Power_VREGAndLSAD(0, 1260), 3);

    // VDDIF
    REPEAT_FUNC(Calibrate_Power_VDDIF(0, VDDIF_ACTIVE_TARGET), 3);

    // VDDA
    REPEAT_FUNC(Calibrate_Power_VDDA(0, VDDA_ACTIVE_TARGET), 3);

    // VDDC
    REPEAT_FUNC(Calibrate_Power_VDDC(VDDC_LIMIT_TARGET, VDDCM_MIN_CP_DELTA_TARGET), 3);

    // Update VDDC
    result |= Calibrate_Support_UpdateVDDC((VDDC_LIMIT_TARGET / 10), 1, 0);

    // VDDC
    REPEAT_FUNC(Calibrate_Power_VDDC(VDDC_ACTIVE_TARGET, VDDCM_MIN_CP_DELTA_TARGET), 3);

    // VDDM
    REPEAT_FUNC(Calibrate_Power_VDDM(VDDM_ACTIVE_TARGET, VDDCM_MIN_CP_DELTA_TARGET), 3);

    // VMIC
    REPEAT_FUNC(Calibrate_Power_VMIC(0, VMIC_ACTIVE_TARGET), 3);

    // VDDOD : 현재 VBAT과 동일한 전원 입력 소스를 사용하고 있음
    REPEAT_FUNC(Calibrate_Power_VDDOD(0, 1260), 3);  // Default : VDDOD_ACTIVE_TARGET (1150)

    /* Update the local RAM instance of the manufacturing information table with the calibrated power measurements. */
    Calibrate_Support_UpdateVREG();
    Calibrate_Support_UpdateLSAD_GAIN();
    Calibrate_Support_UpdateLSAD_OFFSET();

    REPEAT_FUNC(Calibrate_Support_UpdateVDDIF((VDDIF_ACTIVE_TARGET / 10), 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDA((VDDA_ACTIVE_TARGET / 10), 0, 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDC((VDDC_ACTIVE_TARGET / 10), 0, 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDC_CP(VDDCM_MIN_CP_DELTA_TARGET, 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDM((VDDM_ACTIVE_TARGET / 10), 0, 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDM_CP(VDDCM_MIN_CP_DELTA_TARGET, 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVMIC((VMIC_ACTIVE_TARGET / 10), 0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDOD((VDDOD_ACTIVE_TARGET / 10), 0), 1);

    /* Oscillator multiplier limits */

    /* OSC_MULTIPLIER1_LIMIT 30000000 Hz
     * OSC_MULTIPLIER2_LIMIT 60000000 Hz */

    /* 30.72 MHz      = OSC_MULTIPLY_BY_2
     * OSC_MUL_LIMIT1 = SYS_FREQ_29M44
     * OSC_MUL_LIMIT2 = SYS_FREQ_59M52 */

    /* Clock Calibration */
    /* Calibrate_Clock_Initialize() needs to be called before calling Calibrate_Clock_Internal_OSC() function.
     * Calibrate and store three frequencies (7.68 MHz, 15.36 MHz, 30.72 MHz) to the local RAM instance of the manufacturing information table.
     * Hardware Requirement: Provide a square wave @ REF_CLOCK_FREQ (128 Hz) on any selected DIO. */

    REPEAT_FUNC(Calibrate_Clock_Internal_OSC(2560, REF_CLK_DIO), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateCLK(SYS_FREQ_2M56, 2), 1);

    REPEAT_FUNC(Calibrate_Clock_Internal_OSC(30720, REF_CLK_DIO), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateCLK(SYS_FREQ_30M72, 1), 1);

    // REPEAT_FUNC(Calibrate_Clock_Internal_OSC(15360, REF_CLK_DIO), 1)
    // REPEAT_FUNC(Calibrate_Support_UpdateCLK(SYS_FREQ_15M36, 1), 1);

    REPEAT_FUNC(Calibrate_Clock_Internal_OSC(7680, REF_CLK_DIO), 1);

    /* Re-initialize the system clock to default trimming if the calibration fails.
     * The default clock rate of 7.68 MHz is required for the LSAD stabilization delays used in the power calibration */
    REPEAT_FUNC(Calibrate_Support_UpdateCLK(SYS_FREQ_7M68, 0), 1);

    /* Delay to avoid using the LSAD before the clock and the affected power supplies are stable */
    Delay(1);

    /* Initialize system clock frequency variables to default frequency of 7.68 MHz.
     * These variables are used by the system delay functions.*/
#if defined(__chess__)
    SYSVAR_SET_FREQ(SYS_FREQ_7M68);
#else
    SystemInit();
#endif

    /* ADC Offset Calibration */
    /* ADC_Init needs to be called first for each ADC to be calibrated.
     * Use longer than minimum delays and averages to ensure accurate results for uninterrupted execution */
    ADCInit(0);
    REPEAT_FUNC(Calibrate_ADC_Offset(0, (MIN_ADC_STABILIZATION_DELAY * 3), (MIN_ADC_AVERAGES * 3)), 1);

    ADCInit(1);
    REPEAT_FUNC(Calibrate_ADC_Offset(1, (MIN_ADC_STABILIZATION_DELAY * 3), (MIN_ADC_AVERAGES * 3)), 1);

    ADCInit(2);
    REPEAT_FUNC(Calibrate_ADC_Offset(2, (MIN_ADC_STABILIZATION_DELAY * 3), (MIN_ADC_AVERAGES * 3)), 1);

    ADCInit(3);
    REPEAT_FUNC(Calibrate_ADC_Offset(3, (MIN_ADC_STABILIZATION_DELAY * 3), (MIN_ADC_AVERAGES * 3)), 1);

    /* Update the local RAM instance of the manufacturing information table with the ADC offset calibration settings. */
    REPEAT_FUNC(Calibrate_Support_UpdateADC_OFFSET(0), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateADC_OFFSET(1), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateADC_OFFSET(2), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateADC_OFFSET(3), 1);

    /* Re-calibrate VDDA, VDDC, and VDDM with PMU as the reference. */
    REPEAT_FUNC(Calibrate_Power_VDDA_PMURef(0, VDDA_ACTIVE_TARGET), 3);
    REPEAT_FUNC(Calibrate_Power_VDDC_PMURef(VDDC_RETENTION_TARGET, VDDCM_MIN_CP_DELTA_TARGET, &vddc_ret_trim), 3);
    REPEAT_FUNC(Calibrate_Power_VDDM_PMURef(VDDM_STANDBY_TARGET, VDDCM_MIN_CP_DELTA_TARGET, &vddm_ret_trim_standby), 3);
    REPEAT_FUNC(Calibrate_Power_VDDM_PMURef(VDDM_RETENTION_TARGET, VDDCM_MIN_CP_DELTA_TARGET, &vddm_ret_trim_retention), 3);

    /* Update the local RAM instance of the manufacturing information table with the power measurements calibrated
     * against the PMU reference. */
    REPEAT_FUNC(Calibrate_Support_UpdateVDDA((VDDA_ACTIVE_TARGET / 10), 0, 1), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDCByValue((VDDC_RETENTION_TARGET / 10), 0, 1, (uint8_t) (vddc_ret_trim & 0xFF)), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDMByValue((VDDM_STANDBY_TARGET / 10), 0, 1, (uint8_t) (vddm_ret_trim_standby & 0xFF)), 1);
    REPEAT_FUNC(Calibrate_Support_UpdateVDDMByValue((VDDM_RETENTION_TARGET / 10), 1, 1, (uint8_t) (vddm_ret_trim_retention & 0xFF)), 1);

    /* If NVM device can't be accessed, there is no file system on the attached NVM, or if the calibration failed,
     * end the application here with an error since writing the manufacturing table is either not possible or not
     * advisable. */
    if (nvm_access == 0 || result != ERRNO_NO_ERROR)
    {
        ci_handle_result(ERRNO_NVMLIB_WRITE_CAL_ERROR, "NVM device can't be accessed");
    }

    /* Write the trim values to NVM */
    REPEAT_FUNC(Calibrate_Support_WriteManuTable(), 1);

    /* Clear the Manufacturing information table in RAM and then load from NVM to verify the write.
     * If the CRC doesn't match, Calibrate_Support_LoadManuTable will return an error. */
    Calibrate_Support_ClearManuTable();
    REPEAT_FUNC(Calibrate_Support_LoadManuTable(), 1);

    /* Copy the manufacturing information table to the preset location in CM3 PRAM2 (SYSVAR_MANU_TABLE in sk5_*sys.h)
     * using the functions in HAL (trims.c).
     * Next, load the trim values back to the appropriate registers for demonstration. */
    Sys_Trims_LoadManuTable((uint32_t chess_storage(IOMEM) *) manu_table_buffer);
    REPEAT_FUNC(Sys_Trims_SetVREGAndLSAD(), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDIF((VDDIF_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDA((VDDA_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDC((VDDC_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDC_CP(VDDCM_MIN_CP_DELTA_TARGET), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDM((VDDM_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDM_CP(VDDCM_MIN_CP_DELTA_TARGET), 1);
    REPEAT_FUNC(Sys_Trims_SetVDDOD((VDDOD_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetVMIC((VMIC_ACTIVE_TARGET / 10)), 1);
    REPEAT_FUNC(Sys_Trims_SetOperatingFrequency(SYS_FREQ_30M72), 1);
    REPEAT_FUNC(Sys_Trims_SetADCOffsets(), 1);

    /* LSAD calibration for battery */

    // clang-format off
    // SLOWCLK은 반드시 1.28 MHz로 설정
    D_CLK->CFG_1 = ( ADCCLK_PRESCALE_8   | ADCCLK_SRC_SYSCLK  | SDMCLK_PRESCALE_2
    		       | SLOWCLK_PRESCALE_24 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK );

    D_CLK->CFG_2 = (UCLK_PRESCALE_8 | UCLK_SRC_ADCCLK);
    // clang-format on

    Delay(5);  // 클럭 안정화 대기

#if 1
    /* Disable exceptions (except NMI and the hard fault exception) and
     * interrupts before configuring interfaces and peripherals by setting a
     * 1 to the 1-bit interrupt mask register PRIMASK */
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    /* Clear the enable for all of the external interrupts. */
    Sys_NVIC_DisableAllInt();

    /* Clear the pending status for all of the external interrupts. */
    Sys_NVIC_ClearAllPendingInt();

    /* Un-mask exceptions and interrupts by setting a 0 to the 1-bit interrupt
     * mask register PRIMASK */
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    Sys_LSAD_InputConfig(0, LSAD_INPUT_VSSA);
    Sys_LSAD_InputConfig(1, LSAD_INPUT_VMIC);
    Sys_LSAD_InputConfig(2, LSAD_INPUT_VTEMP);
    Sys_LSAD_InputConfig(3, LSAD_INPUT_DIO23);

    g_ci_lsad_sum[0] = 0;
    g_ci_lsad_sum[1] = 0;
    g_ci_lsad_sum[2] = 0;
    g_ci_lsad_sum[3] = 0;
    g_ci_lsad_sum[4] = 0;
    g_ci_lsad_sum[5] = 0;
    g_ci_lsad_sum[6] = 0;
    g_ci_lsad_sum[7] = 0;

    // LSAD->CFG = (LSAD_INT_CH0 | LSAD_INT_ENABLE | LSAD_PRESCALE_3200);
    LSAD->CFG = (LSAD_INT_VDDM | LSAD_INT_ENABLE | LSAD_PRESCALE_3200);

    NVIC_ClearPendingIRQ(LSAD_IRQn);
    NVIC_EnableIRQ(LSAD_IRQn);

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        if (10 < g_ci_lsad_sample_count)
        {
            g_ci_lsad_sample_count_prev = g_ci_lsad_sample_count;
            break;
        }
    }

    SEGGER_RTT_printf(0, "\r\n");
    SEGGER_RTT_printf(0, "%sLSAD%s : \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET);

    for (int i = 0; i < 100; i++)
    {
        uint32_t val[8];

        while (1)
        {
            delay_ms(5);
            SYS_WATCHDOG_REFRESH();

            if (g_ci_lsad_sample_count_prev != g_ci_lsad_sample_count)
            {
                g_ci_lsad_sample_count_prev = g_ci_lsad_sample_count;
                break;
            }
        }

        val[0] = LSAD->DATA_TRIM_SAT_CH[0];
        val[1] = LSAD->DATA_TRIM_SAT_CH[1];
        val[2] = LSAD->DATA_TRIM_SAT_CH[2];
        val[3] = LSAD->DATA_TRIM_SAT_CH[3];
        val[4] = LSAD->DATA_VBAT;
        val[5] = LSAD->DATA_VDDC;
        val[6] = LSAD->DATA_VDDM;

        g_ci_lsad_sum[0] += val[0];  // VSSA
        g_ci_lsad_sum[1] += val[1];  // VMIC
        g_ci_lsad_sum[2] += val[2];  // VTEMP
        g_ci_lsad_sum[3] += val[3];  // DIO23
        g_ci_lsad_sum[4] += val[4];  // VBAT
        g_ci_lsad_sum[5] += val[5];  // VDDC
        g_ci_lsad_sum[6] += val[6];  // VDDM

        SEGGER_RTT_printf(0, "#%2d ", i);

        if (((i + 1) % 20) == 0)
        {
            SEGGER_RTT_printf(0, "\r\n");
        }
        else
        {
            SEGGER_RTT_printf(0, ", ");
        }

        SYS_WATCHDOG_REFRESH();
    }

    for (int i = 0; i < 8; i++)
    {
        g_ci_lsad_sum[i] = g_ci_lsad_sum[i] / 100;
    }

    SEGGER_RTT_printf(0, "%sVSSA%s  = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[0]);
    SEGGER_RTT_printf(0, "%sVMIC%s  = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[1]);
    SEGGER_RTT_printf(0, "%sVTEMP%s = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[2]);
    SEGGER_RTT_printf(0, "%sDIO23%s = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[3]);
    SEGGER_RTT_printf(0, "%sVBAT%s  = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[4]);
    SEGGER_RTT_printf(0, "%sVDDC%s  = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[5]);
    SEGGER_RTT_printf(0, "%sVDDM%s  = %u \r\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET, g_ci_lsad_sum[6]);

    NVIC_DisableIRQ(LSAD_IRQn);
    LSAD->CFG = (LSAD_INT_CH0 | LSAD_INT_DISABLE | LSAD_PRESCALE_3200);

    /* Mount filesystem and write LSAD calibration value */
    // if (NVMReInitOptions(NULL) != ARM_DRIVER_OK)
    if (NVMInit(NULL) != ARM_DRIVER_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "NVMReInitOptions(NULL)");
    }

    SEGGER_RTT_printf(0, "NVMInit(NULL) == ARM_DRIVER_OK \r\n");

    ret = f_mount(&g_ci_mount, "1:", 1);

    if (ret == FR_OK)
    {
        SEGGER_RTT_printf(0, "FILESYSTEM EXIST \r\n");
    }
    else if (ret == FR_NO_FILESYSTEM)
    {
        SEGGER_RTT_printf(0, "NO FILESYSTEM EXIST \r\n");

        ret = f_mkfs("1:", NULL, g_ci_mount.win, FF_MAX_SS);

        if (ret == FR_OK)
        {
            SEGGER_RTT_printf(0, "SUCCESS TO MAKE FILESYSTEM \r\n");

            ret = f_mount(&g_ci_mount, "1:", 1);

            if (ret != FR_OK)
            {
                ci_handle_result(ERRNO_GENERAL_FAILURE, "f_mount(&g_ci_mount, \"1:\", 1)");
            }
        }
        else
        {
            ci_handle_result(ERRNO_GENERAL_FAILURE, "f_mkfs(\"1:\", NULL, g_ci_mount.win, FF_MAX_SS)");
        }
    }
    else
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_mount(&g_ci_mount, \"1:\", 1)");
    }

    ret = f_chdrive("1:");

    if (ret != FR_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_chdrive(\"1:\")");
    }

    ret = f_open(&g_ci_ohdl, "/BATT_CAL", (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    if (ret != FR_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_open(&g_ci_ohdl, \"/LSAD_CAL\", (FA_OPEN_APPEND | FA_READ | FA_WRITE))");
    }

    f_lseek(&g_ci_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_ci_ohdl, 0);
    SEGGER_RTT_printf(0, "PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", "/BATT_CAL");

    uint8_t  file_buf[4];
    int      file_btw;
    int      file_bw;
    uint32_t calibrated_4v;

    calibrated_4v = g_ci_lsad_sum[3] - g_ci_lsad_sum[0];

    file_buf[0] = calibrated_4v & 0xFF;
    file_buf[1] = (calibrated_4v >> 8) & 0xFF;
    file_buf[2] = (calibrated_4v >> 16) & 0xFF;
    file_buf[3] = (calibrated_4v >> 24) & 0xFF;

    file_btw = 4;

    ret = f_write(&g_ci_ohdl, file_buf, file_btw, &file_bw);

    // 안전을 위한 flush
    f_sync(&g_ci_ohdl);

#if 1
    ret = f_stat("/BATT_CAL", &g_fno);
    if (ret == FR_OK)
    {
        SEGGER_RTT_printf(0,
                          "NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                          g_fno.fname,
                          g_fno.fsize,
                          g_ci_ohdl.obj.sclust);
    }
#endif

    f_close(&g_ci_ohdl);

    if (ret != FR_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_write(&g_ci_ohdl, file_buf, file_btw, &file_bw)");
    }

    if (file_bw != file_btw)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "file_bw != file_btw");
    }

    ci_handle_result(ERRNO_NO_ERROR, "UPDATE LSAD CALIBRATION VALUE");

#if 1  // Manufacture table backup (start)
    ret = f_open(&g_ci_ohdl, "/MANUF_TABLE", (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    if (ret != FR_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_open(&g_ci_ohdl, \"/MANUF_TABLE\", (FA_OPEN_APPEND | FA_READ | FA_WRITE))");
    }

    f_lseek(&g_ci_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_ci_ohdl, 0);
    SEGGER_RTT_printf(0, "PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", "/MANUF_TABLE");

    file_btw = MANU_TABLE_SIZE_OCTETS;

    ret = f_write(&g_ci_ohdl, manu_table_buffer, file_btw, &file_bw);

    // 안전을 위한 flush
    f_sync(&g_ci_ohdl);

#if 1
    ret = f_stat("/MANUF_TABLE", &g_fno);
    if (ret == FR_OK)
    {
        SEGGER_RTT_printf(0,
                          "NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                          g_fno.fname,
                          g_fno.fsize,
                          g_ci_ohdl.obj.sclust);
    }
#endif

    f_close(&g_ci_ohdl);

    if (ret != FR_OK)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "f_write(&g_ci_ohdl, manu_table_buffer, file_btw, &file_bw)");
    }

    if (file_bw != file_btw)
    {
        ci_handle_result(ERRNO_GENERAL_FAILURE, "file_bw != file_btw");
    }

    ci_handle_result(ERRNO_NO_ERROR, "UPDATE MANUF TABLE");

#endif  // Manufacture table backup (end)
#endif

    /* Save final status of the application */
    success = (result == ERRNO_NO_ERROR);

    /* Exit application and toggle CI_LED_DIO_GREEN to signify successful or failed execution. */
    ExitApp(result);

    /* Should never get here */
    return ERRNO_GENERAL_FAILURE;
}

void LSAD_IRQHandler(void)
{
    g_ci_lsad_sample_count++;

    if (10 < g_ci_lsad_sample_count)
    {
        // NVIC_DisableIRQ(LSAD_IRQn);
        // LSAD->CFG = (LSAD_INT_CH0 | LSAD_INT_DISABLE | LSAD_PRESCALE_3200);
    }
}

void ExitApp(int status)
{
    if (status == ERRNO_NO_ERROR)
    {
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_GREEN);
        SEGGER_RTT_printf(0, "\r\nCALIBRATION FINISHED SUCCESSFULLY \r\n\n");
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
    }
    else
    {
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_TEXT_BRIGHT_RED);
        SEGGER_RTT_printf(0, "\r\nCALIBRATION FAILED \r\n\n");
        SEGGER_RTT_printf(0, "%s", RTT_CTRL_RESET);
    }

    ci_led_color(CI_LED_COLOR_BLACK);

    while (1)
    {
        /* Toggle CI_LED_DIO_GREEN twice if the application finishes successfully, 4 times otherwise. */
        if (status == ERRNO_NO_ERROR)
        {
            ToggleGPIO(CI_LED_DIO_GREEN, 2, 150);
        }
        else
        {
            ToggleGPIO(CI_LED_DIO_RED, 4, 100);
        }

        /* Refresh the watchdog timer */
        SYS_WATCHDOG_REFRESH();

        /* Wait two seconds in between signals */
        Delay(500);
    }
}

void ToggleGPIO(uint8_t dio, uint8_t n, uint32_t delay_ms)
{
    SYS_WATCHDOG_REFRESH();

    for (; n > 0; n--)
    {
        Sys_GPIO_Toggle(dio);
        Delay(delay_ms);
    }

    SYS_WATCHDOG_REFRESH();
}

void Delay(uint32_t delay_ms)
{
#if defined(__chess__)
    SYSLIB_Delay((uint24_t) delay_ms);
#else
    Sys_Delay(((float) delay_ms / 1000) * SystemCoreClock);
#endif
}
