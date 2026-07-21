/**
 * @file snd1_qcc_control_pins.c
 */

#include <tdc_qcc.h>

#if TDC_QCC_DEBUG_ENABLE
static tdc_qcc_isd_t  s_state_old = TDC_QCC_ISD_DISCONNECTED;
static tdc_qcc_mode_t s_mode_old  = TDC_QCC_MODE_SHUTDOWN;
#endif  // 끝, TDC_QCC_DEBUG_ENABLE

void tdc_qcc_init(void)
{
    Sys_DIO_Config(TDC_QCC_DIO_QCC_CTRL, TDC_QCC_DIO_CFG_GPIO_OUT);
    Sys_DIO_Config(TDC_QCC_DIO_ISD_CHECK, TDC_QCC_DIO_CFG_GPIO_OUT);

    tdc_qcc_set_isd(TDC_QCC_ISD_DISCONNECTED);
    tdc_qcc_set_mode(TDC_QCC_MODE_SHUTDOWN);
}

void tdc_qcc_set_isd(tdc_qcc_isd_t state)
{
#if TDC_QCC_DEBUG_ENABLE
    if (s_state_old != state)
    {
        if (state == TDC_QCC_ISD_CONNECTED)
        {
            tdc_qcc_printi("INDICATE: ISD CONNECTED \r\n");
        }
        else
        {
            tdc_qcc_printi("INDICATE: ISD DISCONNECTED \r\n");
        }

        s_state_old = state;
    }
#endif  // 끝, TDC_QCC_DEBUG_ENABLE

    // 확실한 연결 상태가 아니면,
    // 사실상 의미가 없기 때문에 나머지 상태는 전부 연결 해제 상태로 처리
    switch (state)
    {
        case TDC_QCC_ISD_CONNECTED:
            Sys_GPIO_Set_High(TDC_QCC_DIO_ISD_CHECK);
            break;

        case TDC_QCC_ISD_DISCONNECTED:
            Sys_GPIO_Set_Low(TDC_QCC_DIO_ISD_CHECK);
            break;

        default:
            Sys_GPIO_Set_Low(TDC_QCC_DIO_ISD_CHECK);
            break;
    }
}

void tdc_qcc_set_mode(tdc_qcc_mode_t mode)
{
#if TDC_QCC_DEBUG_ENABLE
    if (s_mode_old != mode)
    {
        if (mode == TDC_QCC_MODE_NORMAL)
        {
            tdc_qcc_printi("MODE: NORMAL \r\n");
        }
        else
        {
            tdc_qcc_printi("MODE: SHUTDOWN \r\n");
        }

        s_mode_old = mode;
    }
#endif  // 끝, TDC_QCC_DEBUG_ENABLE

    // 확실한 동작 상태가 아니면,
    // 사실상 의미가 없기 때문에 나머지 상태는 전부 셧다운 상태로 처리
    switch (mode)
    {
        case TDC_QCC_MODE_NORMAL:
            Sys_GPIO_Set_High(TDC_QCC_DIO_QCC_CTRL);
            break;

        case TDC_QCC_MODE_SHUTDOWN:
            Sys_GPIO_Set_Low(TDC_QCC_DIO_QCC_CTRL);
            break;

        default:
            Sys_GPIO_Set_Low(TDC_QCC_DIO_QCC_CTRL);
            break;
    }
}
