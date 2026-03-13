/**
 * @file snd1_qcc_control_pins.c
 */

#include <snd_qcc.h>

#if SND_QCC_DEBUG_ENABLE
static snd_qcc_isd_t  s_state_old = SND_QCC_ISD_DISCONNECTED;
static snd_qcc_mode_t s_mode_old  = SND_QCC_MODE_SHUTDOWN;
#endif  // 끝, SND_QCC_DEBUG_ENABLE

void snd_qcc_init(void)
{
    Sys_DIO_Config(SND_QCC_DIO_QCC_CTRL, SND_QCC_DIO_CFG_GPIO_OUT);
    Sys_DIO_Config(SND_QCC_DIO_ISD_CHECK, SND_QCC_DIO_CFG_GPIO_OUT);

    snd_qcc_set_isd(SND_QCC_ISD_DISCONNECTED);
    snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);
}

void snd_qcc_set_isd(snd_qcc_isd_t state)
{
#if SND_QCC_DEBUG_ENABLE
    if (s_state_old != state)
    {
        if (state == SND_QCC_ISD_CONNECTED)
        {
            snd_qcc_printi("INDICATE: ISD CONNECTED \r\n");
        }
        else
        {
            snd_qcc_printi("INDICATE: ISD DISCONNECTED \r\n");
        }

        s_state_old = state;
    }
#endif  // 끝, SND_QCC_DEBUG_ENABLE

    // 확실한 연결 상태가 아니면,
    // 사실상 의미가 없기 때문에 나머지 상태는 전부 연결 해제 상태로 처리
    switch (state)
    {
        case SND_QCC_ISD_CONNECTED:
            Sys_GPIO_Set_High(SND_QCC_DIO_ISD_CHECK);
            break;

        case SND_QCC_ISD_DISCONNECTED:
            Sys_GPIO_Set_Low(SND_QCC_DIO_ISD_CHECK);
            break;

        default:
            Sys_GPIO_Set_Low(SND_QCC_DIO_ISD_CHECK);
            break;
    }
}

void snd_qcc_set_mode(snd_qcc_mode_t mode)
{
#if SND_QCC_DEBUG_ENABLE
    if (s_mode_old != mode)
    {
        if (mode == SND_QCC_MODE_NORMAL)
        {
            snd_qcc_printi("MODE: NORMAL \r\n");
        }
        else
        {
            snd_qcc_printi("MODE: SHUTDOWN \r\n");
        }

        s_mode_old = mode;
    }
#endif  // 끝, SND_QCC_DEBUG_ENABLE

    // 확실한 동작 상태가 아니면,
    // 사실상 의미가 없기 때문에 나머지 상태는 전부 셧다운 상태로 처리
    switch (mode)
    {
        case SND_QCC_MODE_NORMAL:
            Sys_GPIO_Set_High(SND_QCC_DIO_QCC_CTRL);
            break;

        case SND_QCC_MODE_SHUTDOWN:
            Sys_GPIO_Set_Low(SND_QCC_DIO_QCC_CTRL);
            break;

        default:
            Sys_GPIO_Set_Low(SND_QCC_DIO_QCC_CTRL);
            break;
    }
}
