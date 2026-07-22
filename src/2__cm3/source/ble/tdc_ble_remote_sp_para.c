

#include <tdc_ble_remote.h>
#include <tdc_stim_para_cal.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_shm.h>
#include <board.h> // 디버깅용
#include <tdc_sys_error.h>
#include <tdc_isd_init.h>

void tdc_ble_remote_read_sp_para(bool startFlag, int command)
{
    static int     flowCounter = 0;
    int            bufferForSPI_tx[BLE_DataPacketSize];
    int            tx_index = 0;
    int            i, k, m, n;
    tdc_sys_error_code_t errorCode;

    static int  stimulDAC_Slope_QI5F12;
    static int  offset_uA;
    static int *p_cfxStimulLevel_255;
    static int  stimulationLevel_uA[df_MaxNumOfElectrode];

    ST_STIUL_DAC_REGISTER_VALUE *stimulDAC_setting;
    int                          connectedISD_num, isd_id;
    int                          adc_inputMax;
    int                          value;

    if (startFlag)
    {

        flowCounter = 0;
        for (i = 0; i < df_MaxNumOfElectrode; i++)
        {
            stimulationLevel_uA[i] = 0;
        }
    }

    switch (flowCounter)
    {

        case 0:
        {
        }

        break;

        case 1:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 입력 ADC 최대값 전송

            adc_inputMax = tdc_shm_read_audio_signal_max();

            bufferForSPI_tx[tx_index++] = (char) (adc_inputMax >> 24);
            bufferForSPI_tx[tx_index++] = (char) ((adc_inputMax >> 16) & (0xFF));
            bufferForSPI_tx[tx_index++] = (char) ((adc_inputMax >> 8) & (0xFF));
            bufferForSPI_tx[tx_index++] = (char) ((adc_inputMax) & (0xFF));

            stimulDAC_setting = tdc_stim_read_dac_register_value();

            // offset DAC 기울기 전송 및 offset 값 계산
            if (stimulDAC_setting->DAC_offsetSlope_register == 0) // 2uA 기울기 오프셋
            {
                bufferForSPI_tx[tx_index++] = offsetDAC_A_Slope_QI4F4;

                value     = offsetDAC_A_Slope_QI5F12 * stimulDAC_setting->DAC_offsetLevel_register;
                offset_uA = value >> 12; // offsetDAC_A 기울기
            }
            else // 4uA 기울기 오프셋
            {

                bufferForSPI_tx[tx_index++] = offsetDAC_B_Slope_QI4F4;

                value     = offsetDAC_B_Slope_QI5F12 * stimulDAC_setting->DAC_offsetLevel_register;
                offset_uA = value >> 12; // offsetDAC_B 기울기
            }
            //  offset level 전송
            bufferForSPI_tx[tx_index++] = stimulDAC_setting->DAC_offsetLevel_register;

            // 자극 DAC 기울기 전송

            switch (stimulDAC_setting->DAC_Slope_register)
            {
                case 0: // 2uA 기울기

                    bufferForSPI_tx[tx_index++] = DAC_A_Slope_QI4F4;
                    stimulDAC_Slope_QI5F12      = DAC_A_Slope_QI5F12;

                    break;
                case 1: // 4uA 기울기

                    bufferForSPI_tx[tx_index++] = DAC_B_Slope_QI4F4;

                    stimulDAC_Slope_QI5F12 = DAC_B_Slope_QI5F12;
                    break;
                case 2: // 6uA 기울기

                    bufferForSPI_tx[tx_index++] = DAC_C_Slope_QI4F4;
                    stimulDAC_Slope_QI5F12      = DAC_C_Slope_QI5F12;
                    break;
                case 3: // 8uA 기울기

                    bufferForSPI_tx[tx_index++] = DAC_D_Slope_QI4F4;
                    stimulDAC_Slope_QI5F12      = DAC_D_Slope_QI5F12;
                    break;
            }

            // 연결된 내부기 ID 전송

            isd_id = tdc_isd_read_connected_id();

            bufferForSPI_tx[tx_index++] = (char) (isd_id >> 24);
            bufferForSPI_tx[tx_index++] = (char) ((isd_id >> 16) & 0xFF);
            bufferForSPI_tx[tx_index++] = (char) ((isd_id >> 8) & 0xFF);
            bufferForSPI_tx[tx_index++] = (char) (isd_id & 0xFF);
        }
        break;
        case 2:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 자극 DAC 레벨 1~18번 채널 값 전송 및 자극출력 전류 uA

            p_cfxStimulLevel_255 = tdc_shm_read_current_stimul_level_255();

            for (k = 0; k < 18; k++)
            {

                bufferForSPI_tx[tx_index++] = p_cfxStimulLevel_255[k];

                value                  = stimulDAC_Slope_QI5F12 * p_cfxStimulLevel_255[k];
                stimulationLevel_uA[k] = offset_uA + (value >> 12);
            }
        }
        break;

        case 3:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            p_cfxStimulLevel_255 = tdc_shm_read_current_stimul_level_255();
            // 자극 DAC 레벨 19~32번 채널 값 전송 및 자자극출력 전류 uA

            for (k = 18; k < 32; k++)
            {
                bufferForSPI_tx[tx_index++] = p_cfxStimulLevel_255[k];

                value                  = stimulDAC_Slope_QI5F12 * p_cfxStimulLevel_255[k];
                stimulationLevel_uA[k] = offset_uA + (value >> 12);
            }
        }
        break;

        case 4:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 자극출력 전류 uA 1~8번 채널 값 전송

            for (k = 0; k < 8; k++)
            {

                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] >> 8;   // 상위 바이트
                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] & 0xFF; // 하위 바이트
            }
        }
        break;

        case 5:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 자극출력 전류 uA 9~16번 채널 값 전송

            for (k = 8; k < 16; k++)
            {

                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] >> 8;   // 상위 바이트
                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] & 0xFF; // 하위 바이트
            }
        }
        break;

        case 6:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 자극출력 전류 uA 17~24번 채널 값 전송

            for (k = 16; k < 24; k++)
            {

                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] >> 8;   // 상위 바이트
                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] & 0xFF; // 하위 바이트
            }
        }
        break;

        case 7:
        {

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[tx_index++] = command;

            // data index 전송
            bufferForSPI_tx[tx_index++] = flowCounter;

            // 자극출력 전류 uA 25~32번 채널 값 전송

            for (k = 24; k < 32; k++)
            {

                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] >> 8;   // 상위 바이트
                bufferForSPI_tx[tx_index++] = stimulationLevel_uA[k] & 0xFF; // 하위 바이트
            }
        }
        break;
    }

    if(flowCounter!=0)
    {
    // nrf 전달
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx,tx_index);
    }

    if(flowCounter==7)
    {
        //  명령 종료
        tdc_ble_remote_clear_command();

    }

    flowCounter++;
}
