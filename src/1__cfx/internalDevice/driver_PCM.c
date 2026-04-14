/**
 * @file driver_PCM.c
 */

#include <driver_PCM.h>
#include <main.h>

int _XMEM g_pcmPreambleState = PCM_PREAMBLE_STATE_FILL_ZERO;

void pcmDataOut(void)
{
    switch (g_pcm_mode)
    {
        case PcmBitStream_Mode_Preamble:
            fn_PcmBitStream_Mode_Preamble();
            break;

        case PcmBitStream_Mode_LiveStimulation:
            fn_PcmBitStream_Mode_LiveStimulation();
            break;

        case PcmBitStream_Mode_SepcificCommand:
            fn_PcmBitStream_Mode_SepcificCommand();
            break;

        case PcmBitStream_Mode_NopStandby:
            fn_PcmBitStream_Mode_NopStandby();
            break;

        case PcmBitStream_Mode_NopBacktel:
            fn_PcmBitStream_Mode_NopBacktel();
            break;

        default:
            fn_PcmBitStream_Mode_FillZero();
            break;
    }

    return;
}

void fn_PcmBitStream_Mode_Preamble(void)
{
    int _XMEM *p_pcmBuffer = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

    if (g_pcmPreambleState == PCM_PREAMBLE_STATE_FILL_ZERO)  // fill with zero
    {
        for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
            chess_loop_range(df_MaxNumTransferableChannel, df_MaxNumTransferableChannel)
            {
                *p_pcmBuffer = 0;
                p_pcmBuffer--;
            }

        g_pcmPreambleState = PCM_PREAMBLE_STATE_TOGGLE;  // addr_PCM_preableState 값 토글
    }
    else  // fill with preamble
    {
        for (register int i = 0; i < (df_MaxNumTransferableChannel - 1); i++)
            chess_loop_range((df_MaxNumTransferableChannel - 1), (df_MaxNumTransferableChannel - 1))
            {
                *p_pcmBuffer = 0xFFFFF;
                p_pcmBuffer--;
            }

        *p_pcmBuffer = pcm_Mold_Preamble;  // 마지막 word는 preamble 데이터

        // PCM 모드가 preable로 유지되어 있으면 다시 0이 출력된다. 모드를 NopStandby로 변경한다.
        Addr_SharedMem->cfx_PCM_interface.PCM_mode = PcmBitStream_Mode_NopStandby;

        g_pcmPreambleState = PCM_PREAMBLE_STATE_FILL_ZERO;  // addr_PCM_preableState 값 토글
    }
}

void fn_PcmBitStream_Mode_LiveStimulation(void)
{
    // 자극 출력 PCM
    stimulationStrategy();

    // 백텔을 사용한 내부기 연결 확인
#ifndef DisalbedBackTel
    check_link_connection_state();
#endif
}

void fn_PcmBitStream_Mode_SepcificCommand(void)
{
    int chess_storage(XMEM) * p_pcmBuffer;

    Addr_SharedMem->is_pcm_specific_command_reading = 1;  // CM3와의 SpecificCommand 경쟁 상태 방지를 위한 코드 by 김은수

    p_pcmBuffer = (int chess_storage(XMEM) *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(df_MaxNumTransferableChannel, df_MaxNumTransferableChannel)
        {
            *p_pcmBuffer = Addr_SharedMem->cfx_PCM_interface.PCM_specificBuffer[i];
            p_pcmBuffer--;

            Addr_SharedMem->pcm_specific_command_read_index = i;  // CM3와의 SpecificCommand 경쟁 상태 방지를 위한 코드 by 김은수
        }

    // 특정 명령 후에 CM3에서 즉각적으로 다음 PCM 출력을 제어할 수 없는 타이밍이 발생할 경우를 대비하여 다음 PCM모드를 설정한다.
    // CM3에서 해당 타이밍에 제어가 가능하면 그 값으로 덮어써진다.
    Addr_SharedMem->cfx_PCM_interface.PCM_mode = Addr_SharedMem->cfx_PCM_interface.PCM_mode_next;

    Addr_SharedMem->is_pcm_specific_command_reading = 0;  // CM3와의 SpecificCommand 경쟁 상태 방지를 위한 코드 by 김은수
}

void fn_PcmBitStream_Mode_NopStandby(void)
{
    int chess_storage(XMEM) * p_pcmBuffer;

    p_pcmBuffer = (int chess_storage(XMEM) *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(df_MaxNumTransferableChannel, df_MaxNumTransferableChannel)
        {
            *p_pcmBuffer = pcm_Mold_NopStandby;
            p_pcmBuffer--;
        }
}

void fn_PcmBitStream_Mode_NopBacktel(void)
{
#ifdef LED_B_pin_CFX_test
    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B)
#endif

    int chess_storage(XMEM) * p_pcmBuffer;

    p_pcmBuffer = (int chess_storage(XMEM) *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(df_MaxNumTransferableChannel, df_MaxNumTransferableChannel)
        {
            *p_pcmBuffer = pcm_Mold_NopBacktel;
            p_pcmBuffer--;
        }
}

void fn_PcmBitStream_Mode_FillZero(void)
{
    int chess_storage(XMEM) * p_pcmBuffer;

    p_pcmBuffer = (int chess_storage(XMEM) *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(df_MaxNumTransferableChannel, df_MaxNumTransferableChannel)
        {
            *p_pcmBuffer = 0;
            p_pcmBuffer--;
        }
}

void fn_reset_PCM(void)
{
    // 변수 초기화
    g_pcmPreambleState = PCM_PREAMBLE_STATE_FILL_ZERO;

    // 초기화 명령은 CM3에서 받은 것이라서, CM3로 명령이 수행되었음을 알려준다.
    Addr_SharedMem->cfx_PCM_interface.Reset_PCM = 0;
}
