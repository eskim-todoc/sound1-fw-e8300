#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>

#include <board.h>
#include <tdc_isd_fpga.h>
#include <internalStimulationChip.h>
#include <tdc_isd.h>
#include <tdc_hal_i2c_isd.h>
#include <tdc_stim_definitions.h>

#include <tdc_isd.h>
#include <tdc_isd_init_fpga.h>
#include <tdc_shm.h>
#include <tdc_stim_common.h>

#include <tdc_sys_error.h>
#include <tdc_led_output.h>

#define df_masterISD_Serial      0x0000FFFF
#define lenght_MANUFACTURER_NAME 9

const char MANUFACTURER_NAME[] = "TODOC_OTE";
static int isd_id;

int tdc_isd_read_connected_id(void)
{
    return isd_id;
}

void tdc_isd_path_open(bool isdControlStateChagedFlag)
{
    static int          temporal_registerValue;  // PCM으로 출력한 설정 레지스터 값.. I2C로 확인 전 임시 저장용
    static int          flowControlCounter = 0;
    static int          isd_id_match_num   = 0;
    static bool         isNormalUser       = true;
    int                 isd_passKey        = 0;
    int                 isd_passKeySlice   = 0;
    int                 r_FPGA_registerValue;
    int                 w_isd_registerValue;
    int                 r_FPGA_FIFO_buff[df_FPGA_FIFO_buffSize];
    int                 FIFO_CounterValue;
    int                 w_isd_registerSlice;
    int                 isd_id_onFlash;
    int                 pcm_index;
    int                 i;
    bool                FPGA_error;
    bool                FPGA_FIFO_empty;
    int                *p_userName;
    ST__MAPPING_PACKET *p_mappingPacket;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        tdc_isd_clear_control_state_changed_flag();
        isd_id_match_num = 0;
    }

    pcm_index = 0;

    // FPGA FIFO를 지운 후,
    // 연결된 내부기의 ID를 읽어 들이고 매핑 데이터에 동일한 ID를 보유하고 있는지 확인하고
    // 해당 ID의 passkey를 내부기에 전달하고 패스가 열렸는지 확인한다.

    switch (flowControlCounter)
    {
        case 0:  // FPGA 에러 발생 상태 확인 후 PCM 펄스폭 최소로 설정 (이 case에서 PCM 모드 변경 예정)
        {
            tdc_isd_fpga_check_fpga_pcm_error(&FPGA_error);

            if (FPGA_error)
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  // FPGA 에러 발생, FPGA 초기화
                TDC_PRINTF_E("[FPGA] ERROR OCCURRED \r\n");
            }
            else
            {
                tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);

                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
                }

                // 앞선 tdc_isd_step 함수 과정에서
                // tdc_isd_init_device() 함수를 통해 내부기 칩의 전원 레벨을 읽고 난 후 이 함수를 수행하기 때문에
                // 마지막 PCM 동작 모드는 NopStandby 일 것이다.
                // 그래서 여기서는 바로 SpecificCommand를 적용시켜도 정상 동작한다.

                tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
            }

            tdc_isd_set_i2c_free();
        }
        break;

        case 10:  // 펄스폭 최소 설정 확인
        {
            tdc_isd_set_i2c_busy();

            if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
                {
                    // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
                    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(r_FPGA_registerValue);
                }
                else
                {
                    // 펄스폭 설정 실패, FPGA 초기화
                    tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, 0);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                    TDC_PRINTF_E("[FPGA] FAILED TO SET PULSE PHASE WIDTH TO MINIMUM \r\n");
                }
            }
        }
        break;

        case 11:  // backtel 8bit로 변경 (앞선 PCM 모드 변경 후 약 10ms 이후, 이 case에서 PCM 모드 변경 예정)
        {
            temporal_registerValue = tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);

            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 15:  // PCM 출력에 대한 FPGA 상태 읽기
        {
            if (tdc_isd_fpga_read_backtel_config(&r_FPGA_registerValue))
            {
                if (temporal_registerValue == r_FPGA_registerValue)
                {
                    // 위에서, PCM으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지 확인 후 업데이트
                    tdc_isd_fpga_update_fpga_backtel_config_written_value(temporal_registerValue);

                    // FIFO를 지우고 FPGA 상태를 읽어 본다.
                    if (tdc_isd_fpga_write_clear_fifo())
                    {
                        if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // FIFO 지워 졌는지 확인.
                        {
                            if (!FPGA_FIFO_empty)
                            {
                                tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, 0);
                                tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                                TDC_PRINTF_E("[FPGA] FIFO IS NOT CLEARED \r\n");
                            }
                        }
                    }
                }
                else
                {
                    // 백텔 레지스터 설정 오류
                    tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, 0);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                    TDC_PRINTF_E("[FPGA] BACKTEL REGISTER IS NOT CONFIGURED \r\n");
                }
            }
        }
        break;

        case 16:  // 내부기 칩 소프트웨어 리셋 (앞선 PCM 모드 변경 후 약 5ms 이후, 이 case에서 PCM 모드 변경 예정)
        {
            // cipher FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_SystemClkReset;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x01;  // reset
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 약 4ms 동안 내부기 칩이 소프트웨어 리셋이 걸렸으리라 판단
        // 'h10 레지스터의 SYSCLK_OE을 1로 설정하여 자극 10V PMIC 활성화
        case 20:  // (앞선 PCM 모드 변경 후 약 4ms 이후, 이 case에서 PCM 모드 변경 예정)
        {
            // LGA 패키지 설정
#if defined(EEPROM_LSK_Error)
            // ISD LSK 설정
            w_isd_registerValue = ISD_registerAddr_LSK_Clk_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x08;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            w_isd_registerValue = ISD_registerAddr_PPSK_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x20;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
#endif

            // ISD SYSCLK_OE 설정
            w_isd_registerValue = ISD_registerAddr_IO_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x08;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // (앞선 PCM 모드 변경 후 약 2ms 이후, 이 case에서 PCM 모드 변경 예정)
        case 22:  // PCM 동작 모드 - 내부기 ID  읽기
        {
            // PCM 데이터
#ifndef DisalbedBackTel
            // cipher FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x80;  // CHIP_ID_FIFO_RDDATA_INDEXdp 아무값이나 쓰면 FIFO가 지워진다.
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD id 읽기
            w_isd_registerValue = ISD_registerAddr_chip_ID;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            // id 읽기 1byte
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

            // id 읽기 2byte
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

            // id 읽기 3byte
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

            // id 읽기 4byte
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
#endif
            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 26:  // I2C 읽기 - 내부기 ID를 읽어서 플래쉬의 ID와 동일한지 확인
        {
#ifndef DisalbedBackTel
            // FIFO 카운터를 읽어 본다.
            if (tdc_isd_fpga_read_fifo_counter(&FIFO_CounterValue))
            {
                if (FIFO_CounterValue == NumByte_ISD_chip_ID)
                {
                    for (i = 0; i < df_FPGA_FIFO_buffSize; i++)
                    {
                        r_FPGA_FIFO_buff[i] = 0;
                    }

                    if (tdc_isd_fpga_read_backtel_fifo(r_FPGA_FIFO_buff, NumByte_ISD_chip_ID))
                    {
                        // 외부기에 사용자의 맵데이터 들어 있지 않고 제조사 맵이 들어 있는 경우 모든 내부기와 연결이 가능하도록 한다.
                        // 외부기에 사용자의 맵 데이터가 들어 있는 경우 사용자의 맵데이터의 내부기 정보와 동일할 경우에만 사용이 가능하도록 한다.
                        // Flash 메모리에 저장되어 있는 ID들 중에서 내부기에서 읽어온 ID 값과 동일한 것을 검색

                        isd_id = 0;

#if 1  // 내부기의 ID를 읽으면 역순(LSB)으로 읽혀저 온다.
                        isd_id = isd_id | r_FPGA_FIFO_buff[NumByte_ISD_chip_ID - 1];
                        isd_id = isd_id << 8;

                        isd_id = isd_id | r_FPGA_FIFO_buff[NumByte_ISD_chip_ID - 2];
                        isd_id = isd_id << 8;

                        isd_id = isd_id | r_FPGA_FIFO_buff[NumByte_ISD_chip_ID - 3];
                        isd_id = isd_id << 8;

                        isd_id = isd_id | r_FPGA_FIFO_buff[NumByte_ISD_chip_ID - 4];
#else
                        isd_id = isd_id | (r_FPGA_FIFO_buff[i] << 8);
#endif

                        if (isd_id != 0)
                        {
                            TDC_PRINTF_I("[ISD] CURRENTLY CONNECTED ISD SERIAL : 0x%08X \r\n", isd_id);

                            p_mappingPacket = (ST__MAPPING_PACKET *) tdc_ble_mapping_get_packet();

                            // EEPROM에 쓰여진 데이터를 사용할 경우
                            if (p_mappingPacket->command != en__mapping_write_original_ISD_N_USER)
                            {
                                isd_id_match_num = 0;
                                isNormalUser     = false;
                                p_userName       = tdc_shm_read_connected_isd_user_name(1);  // 첫번째 사용자 이름

                                TDC_PRINTF_V("[ISD] ISD[1] USER NAME : ");
#if TDC_PRINTF_ENABLE_VERBOSE
                                for (int iLoop = 0; iLoop < 25; iLoop++)
                                {
                                    if (p_userName[iLoop] != 0)
                                    {
                                        TDC_PRINTF_V("%c", p_userName[iLoop]);
                                    }
                                    else
                                    {
                                        TDC_PRINTF_V("\r\n");
                                        break;
                                    }
                                }
#endif

                                for (i = 0; i < lenght_MANUFACTURER_NAME; i++)
                                {
                                    // 첫번째 사용자 이름이 TODOC_OTE가 아니다.
                                    // 즉 매핑프로그램으로 사용자 이름이 저장되어 있다.

                                    if (MANUFACTURER_NAME[i] != (char) p_userName[i])
                                    {
                                        isNormalUser = true;
                                        break;
                                    }
                                }

                                //  마스트 내부기와 연결된 경우.
                                if ((isd_id & 0x0000FFFF) == df_masterISD_Serial)
                                {
                                    isNormalUser = false;
                                    TDC_PRINTF_W("[ISD] CURRENTLY CONNECTED ISD IS MASTER KEY DEVICE \r\n");
                                }

                                if (isNormalUser)
                                {
                                    for (i = 0; i < MaxNumUser; i++)
                                    {
                                        isd_id_onFlash = tdc_shm_read_isd_manufacture_id(i);

                                        if (isd_id == isd_id_onFlash)
                                        {
                                            isd_id_match_num = i + 1;
                                        }
                                    }

                                    // Flash에 저장된 ID와 대응되지 않는다.
                                    if (isd_id_match_num == 0)
                                    {
                                        // 내부기 연결을 끊고 다시 찾아야 함
                                        tdc_sys_error_update(en__EN__ISD_ERROR, en__No_Matched_ISD_ID, 0);
                                        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  //

                                        TDC_PRINTF_D("[ISD] NO MATCHED ISD ID IN FLASH \r\n");
                                    }
                                }
                                else
                                {
                                    isd_id_match_num = ManufacturingDefault_ISD_No;
                                }
                            }
                            else  // 매핑 패킷에 의해 ID 확인이 필요한 경우
                            {
                                /* EEPROM의 데이터가 아니라 매핑과정에서 처음으로 내부기 ID를 적을 때
                                 * 청각사가 입력한 ID와 실제 내부기의 ID를 확인하는 과정이 필요함. */

                                if ((isd_id & 0x0000FFFF) != df_masterISD_Serial)
                                {
                                    if (isd_id == p_mappingPacket->rx_orignal_ISD_info.ISD_id)
                                    {
                                        p_mappingPacket->rx_orignal_ISD_info.id_match = true;
                                    }
                                    else
                                    {
                                        p_mappingPacket->rx_orignal_ISD_info.id_match = false;
                                    }
                                }
                                else  //  마스트키 내부기와 연결되었을때는 내부기 ID체크를 무시한다
                                {
                                    p_mappingPacket->rx_orignal_ISD_info.id_match = true;
                                }

                                p_mappingPacket->rx_orignal_ISD_info.id_check_is_completed = true;

                                // 매핑 프로그램에서 수신된 최초 설정용 내부기 ID값이 현재 내부기 ID와 같은지 확인하는 절차가
                                // 완료 되었기 때문에 현재 EEPROM에 설정되어 있는 맵 데이터로 다시 내부기 설정을 완료한다.
                                tdc_isd_change_state(en__isdStatus_ISD_Power_Ok);  //
                            }
                        }
                        else
                        {
                            tdc_sys_error_update(en__EN__ISD_ERROR, en__ISD_EEPROM_ValueZero, 0);
                            tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.

                            TDC_PRINTF_W("[ISD] CURRENTLY CONNECTED ISD SERIAL : 0x%08X \r\n", isd_id);
                        }
                    }
                }
                else
                {
                    // 읽어온 Backtel 갯수가 적다.
                    if (FIFO_CounterValue == 0)
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackTelCounterZero, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO READ ISD SERIAL (COUNT 0) \r\n");
                    }
                    else
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackterDataLengthError, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO READ ISD SERIAL (NOT ENOUGH BACKTEL) \r\n");
                    }

                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                }
            }
            else
            {
            }
#else
            isd_id           = 0x12345678;
            isd_id           = 0x15910001;
            isd_id_match_num = 1;
            connected_ISD_id = isd_id;
#endif
        }
        break;

        // (앞선 PCM 모드 변경 후 약 5ms 이후, 이 case에서 PCM 모드 변경 예정)
        case 27:  // PCM 출력 - 해당 ID의 passkey를 내부기에 전달하고, Path가 열렸는지 확인한다.
        {
#ifndef DisalbedBackTel
            if (isNormalUser)
            {
                isd_id_onFlash = tdc_shm_read_isd_manufacture_id(isd_id_match_num - 1);
                isd_passKey    = isd_id_onFlash ^ df_encryptionKey;  // passkey 생성
            }
            else
            {
                isd_passKey = isd_id ^ df_encryptionKey;  // 공장초기 맵 데이터 적용.
            }

            // PCM 데이터 (PassKey 전달)

            // ISD passkey 전달용
            w_isd_registerSlice = ISD_registerAddr_ciper_data;
            w_isd_registerSlice = w_isd_registerSlice << 1;
            w_isd_registerSlice = w_isd_registerSlice | ISD_writeRegister;
            w_isd_registerSlice = w_isd_registerSlice << 8;

            // passkey 최하위 바이트
            isd_passKeySlice    = isd_passKey & 0x000000FF;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 하위 바이트
            isd_passKeySlice    = isd_passKey & 0x0000FF00;
            isd_passKeySlice    = isd_passKeySlice >> 8;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 상위 바이트
            isd_passKeySlice    = isd_passKey & 0x00FF0000;
            isd_passKeySlice    = isd_passKeySlice >> 16;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 최상위 바이트
            isd_passKeySlice    = isd_passKey >> 24;  // 부호비트의 전달을 방지하기 위해서 쉬프트하고 0x00FF로 AND
            isd_passKeySlice    = isd_passKeySlice & 0x000000FF;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // ISD 연산을 위해서 1프레임 쉼
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

            // 내부기 열렸는지 읽기
            tdc_isd_fill_pcm_path_open_normal(&pcm_index);

            // 나머지 버퍼는  NOP-Backtel
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopBacktel);
            }

#else
            isd_passKey = isd_id ^ df_encryptionKey;

            // PCM 동작 모드 -  내부기 ID 읽기
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);

            // PCM 데이터

            // PassKey 전달;;
            // ISD passkey 전달용
            w_isd_registerSlice = ISD_registerAddr_ciper_data;
            w_isd_registerSlice = w_isd_registerSlice << 1;
            w_isd_registerSlice = w_isd_registerSlice | ISD_writeRegister;
            w_isd_registerSlice = w_isd_registerSlice << 8;

            // passkey 최하위 바이트
            isd_passKeySlice    = isd_passKey & 0x000000FF;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 하위 바이트
            isd_passKeySlice    = isd_passKey & 0x0000FF00;
            isd_passKeySlice    = isd_passKeySlice >> 8;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 상위 바이트
            isd_passKeySlice    = isd_passKey & 0x00FF0000;
            isd_passKeySlice    = isd_passKeySlice >> 16;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // passkey 최상위 바이트
            isd_passKeySlice    = isd_passKey >> 24;  // 부호비트의 전달을 방지하기 위해서 쉬프트하고 0x00FF로 AND
            isd_passKeySlice    = isd_passKeySlice & 0x000000FF;
            w_isd_registerValue = w_isd_registerSlice | isd_passKeySlice;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // ISD 연산을 위해서 1프레임 쉼
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

            // fill_pcmBuff_writeData_checkPathOpen_normalValue(pcm_index++);
            // fill_pcmBuff_readData_checkPathOpen(pcm_index++);

            // 나머지 버퍼는  NOP-Backtel
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }
#endif
            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // (앞선 PCM 모드 변경 후 약 4ms 이후, 이 case에서 PCM 모드 변경 예정)
        case 31:  // I2C 읽기 - Path 오픈 후 백텔 응답 검증 후 0으로 채워진 값으로 백텔 검증을 한 번 더 진행
        {
#ifndef DisalbedBackTel
            if (tdc_isd_fpga_read_fifo_counter(&FIFO_CounterValue))
            {
                if (FIFO_CounterValue == 1)
                {
                    if (tdc_isd_fpga_is_arbitrary_value_matched_normal_value())
                    {
                        // 백텔이 들어오지 않을 확률이 높은 데이터 조합으로 백텔을 확인하고, 캘리브레이션을 진행한다.
                        tdc_isd_fill_pcm_path_open_dup_zero(&pcm_index);

                        // 나머지 버퍼는 NOP backtel
                        for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                        {
                            tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopBacktel);
                        }

                        tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
                        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
                    }
                    else
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__No_Matched_ISD_ID, __LINE__);
                        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기와 연결을 끊고 다시 시도

                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH NORMAL CHECKING VALUE \r\n");
                    }
                }
                else
                {
                    // 읽어온 Backtel 갯수가 적다.
                    if (FIFO_CounterValue == 0)
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackTelCounterZero, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH NORMAL CHECKING VALUE (COUNT 0) \r\n");
                    }
                    else
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackterDataLengthError, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH NORMAL CHECKING VALUE (TOO MANY BACKTEL) \r\n");
                    }

                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                }
            }

#else
            tdc_sys_error_clear_all();
            tdc_isd_change_state(en__isdStatus_ISD_pathOpen_Ok);
            tdc_shm_change_connected_isd_num_cfx(isd_id_match_num);  // CFX와의 공유 메모리에 연결된 내부기 번호(1~4)를 알려주어서
                                                            // CFX에서 해당 맵데이터를 읽어 올 수 있도록 한다.
            update_Connected_ISD_id(isd_id);
#endif
        }
        break;

        case 35:  // I2C 읽기 - 0으로 채워진 값으로 백텔 검증이 통과하면, 해당 ISD 번호가 진짜 연결된 것으로 간주함
        {
            if (tdc_isd_fpga_read_fifo_counter(&FIFO_CounterValue))
            {
                if (FIFO_CounterValue == 1)
                {
                    if (tdc_isd_fpga_is_arbitrary_value_matched_duplicate_zero_data())
                    {
                        // 정상
                        tdc_sys_error_clear_all();
                        tdc_isd_change_state(en__isdStatus_ISD_pathOpen_Ok);
                        // CFX와의 공유 메모리에 연결된 내부기 번호(1~4)를 알려주어서
                        // CFX에서 해당 맵데이터를 읽어 올 수 있도록 한다.

                        //
                        // 주의: 매핑 연결된 상태에선 맵 로드를 하지 않음, 다만 연결된 ISD 번호는 알려줌
                        //
                        if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode != en__mappingMode)
                        {
                            tdc_shm_change_connected_isd_num_cfx(isd_id_match_num);
                        }
                        else
                        {
                            cfx_cm3_sharedMemoryAll.connected_ISD_num = isd_id_match_num;
                        }

                        TDC_PRINTF_V("[ISD] ISD ID MATCH NUM             = %d \r\n", isd_id_match_num);
                        TDC_PRINTF_V("[ISD] USER SETTING: MAP NUM        = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.mapNum);
                        TDC_PRINTF_V("[ISD] USER SETTING: BLE ON/OFF     = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.Ble_Onff);
                        TDC_PRINTF_V("[ISD] USER SETTING: STIM INDICATOR = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.indicatorStimul_OnOff);
                        TDC_PRINTF_V("[ISD] USER SETTING: LED  INDICATOR = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.indicatorLED_OnOff);
                        TDC_PRINTF_V("[ISD] USER SETTING: TELECOIL       = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.teleCoil_OnOff);
                        TDC_PRINTF_V("[ISD] USER SETTING: AUDIO VOLUME   = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.audioVolume);
                        TDC_PRINTF_V("[ISD] USER SETTING: STIM  VOLUME   = %d \r\n", cfx_cm3_sharedMemoryAll.userSettingValue.stimulVolume);

                        tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                        tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                        tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                        TDC_PRINTF_I("[ISD] SUCCESS TO OPEN ISD PATH \r\n");
                    }
                    else
                    {
                        // 제로가 많이 들어가 값에 대해서 백텔이 오류난다.
                        tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_BackTelDecodingCalibation_error, 0);
                        tdc_isd_change_state(en__isdStatus_FPGA_Ok);

                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH DUPLICATE ZERO CHECKING DATA \r\n");
                    }
                }
                else
                {
                    // 읽어온 Backtel 갯수가 적다.
                    if (FIFO_CounterValue == 0)
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackTelCounterZero, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH DUPLICATE ZERO CHECKING DATA (COUNT 0) \r\n");
                    }
                    else
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackterDataLengthError, 0);
                        TDC_PRINTF_W("[ISD] FAILED TO PATH OPEN WITH DUPLICATE ZERO CHECKING DATA (TOO MANY BACKTEL) \r\n");
                    }

                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                }
            }
        }
        break;

        default:
            break;
    }

    flowControlCounter++;
}

void tdc_isd_enable_stimul_10v(bool isdControlStateChagedFlag)
{
    static int flowControlCounter = 0;
    static int iterationCouter    = 0;
    static int writenBacktelRegisterValue;

    int  r_isd_registerValue, w_isd_registerValue, r_FPGA_registerValue;
    int  vtg_Lock_state;
    int  i, pcm_index = 0;
    bool FPGA_error;
    bool FPGA_FIFO_empty;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        tdc_isd_clear_control_state_changed_flag();
        iterationCouter = 0;
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
            // FPGA 에러 발생 상태 확인 후
            // PCM 펄스폭 최소 및 백텔 레지스터 8비트 모드로 설정 (이 case에서 PCM 모드 변경 예정)
        case 0:
        {
            tdc_isd_fpga_check_fpga_pcm_error(&FPGA_error);

            if (FPGA_error)
            {
                // FPGA 에러 발생, FPGA 초기화
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                TDC_PRINTF_E("[FPGA] FPGA ERROR \r\n");
            }
            else
            {
                tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);

                tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

                writenBacktelRegisterValue = tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);

                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
                }

                // 앞선 tdc_isd_step 함수 과정에서
                // tdc_isd_path_open() 함수를 통해 내부기 칩의 암호키 복호화 및 Forward Check 레지스터 검증까지 완료되었으므로
                // 마지막 PCM 동작 모드는 NopStandby 일 것이다.
                // 그래서 여기서는 바로 SpecificCommand를 적용시켜도 정상 동작한다.

                tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
            }
        }
        break;

        case 4:  // 펄스폭 최소 설정 및 백텔 레지스터 8비트 모드 설정 확인
        {
            if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
                {
                    // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
                    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(r_FPGA_registerValue);
                }
                else
                {
                    // 펄스폭 설정 실패 , FPGA 초기화
                    tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                    TDC_PRINTF_E("[FPGA] FAILED TO SET PULSE PHASE WIDTH TO MINIMUM \r\n");
                }
            }

            if (tdc_isd_fpga_read_backtel_config(&r_FPGA_registerValue))
            {
                if (writenBacktelRegisterValue == r_FPGA_registerValue)
                {
                    // PCM 으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지확인 후 업데이트
                    tdc_isd_fpga_update_fpga_backtel_config_written_value(writenBacktelRegisterValue);

                    if (tdc_isd_fpga_write_clear_fifo())  // 피포를 지우고
                    {
                        // FPGA 상태를 읽어 본다.
                        if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
                        {
                            if (!FPGA_FIFO_empty)
                            {
                                tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, __LINE__);
                                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
                                TDC_PRINTF_E("[FPGA] FAILED TO EMPTY BACKTEL FIFO \r\n");
                            }
                        }
                    }
                }
                else
                {
                    // 백텔 레지스터 설정 오류
                    tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                    TDC_PRINTF_E("[FPGA] FAILED TO SET BACKTEL REGISTER VALUE \r\n");
                }
            }
        }
        break;

        // (앞선 PCM 모드 변경 후 약 5ms 이후, 이 case에서 PCM 모드 변경 예정)
        // VTG_UP_START가 1로 설정되면 칩 외부 10V 스텝업 컨버터를 활성화시킨다. (칩 외부 핀으로 여기서 설정 값 즉, 1이 출력됨)
        // 여기서 칩 외부의 10V를 켜주는 것임
        // 칩 외부에서 만들어져서 입력되는 10V가 안정적인 경우 VTG_LOCK 비트가 1이 되는데,
        // VTG_LOCK_ENABLE가 1로 설정되면, 칩에서 자극을 발생시키는 자극발생부가 위의 VTG_LOCK이 1이여야 활성화되는 설정이다.
        // 즉, VTG_UP_START로 외부 10V를 켜주고, 이 10V가 안정적으로 칩에게 입력되는지
        // VTG_LOCK 비트를 검증해서 1 즉, 안정적이어야 자극발생부가 활성화되도록 VTG_LOCK_ENABLE을 1로 설정하는 것이다.
        case 5:
        {
            // ISD_registerAddr_Stimul_10V 설정 쓰기
            w_isd_registerValue = ISD_registerAddr_Stimul_10V;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x0C;  // 01100 : VTG_UP_START, VTG_LOCK_ENABLE
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는  NOP standby

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

            tdc_isd_set_i2c_free();
        }
        break;

        // (앞선 PCM 모드 변경 후 약 19ms 이후, 이 case에서 PCM 모드 변경 예정)
        // 앞선 설정으로 내부기 칩의 10V가 잘 켜졌는지, VTG_LOCK 비트를 확인하기 위한 백텔 읽기 전송
        case 24:
        {
            tdc_isd_set_i2c_busy();

            // ISD_registerAddr_Stimul_10V 설정 읽기
            w_isd_registerValue = ISD_registerAddr_Stimul_10V;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // VTG_LOCK 비트가 1로 설정되었는지 백텔 데이터를 확인한다.
        // 백텔 수가 1이 아니거나, 백텔 에러가 발생하면
        // 내부기 10Mhz 캐리어를 멈췄다 시작하는, 내부기 전송 파워 설정부터 다시 시작한다.
        // 주의:
        // 아직 (약 4ms 내에) VTG_LOCK 비트가 1로 설정되지 않은 경우,
        // 최대 약 10ms 동안 1ms 마다 백텔 FIFO를 다시 읽는 로직을 구현한거 같지만
        // 실제로는 로직이 한 번 읽은 후 VTG_LOCK 비트가 1이 아닌 경우 백텔 읽기를 다시 수행하는게 아니기 때문에
        // 의미가 없는 10ms 로직으로 동작한다. 실제 동작에는 문제가 없어서 레거시 코드로 남겨둔다.
        case 28:
        {
#ifndef DisalbedBackTel
            // FIFO 카운터를 읽어 본다.
            if (tdc_isd_fpga_read_fifo_counter(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == 1)
                {
                    if (tdc_isd_fpga_read_backtel_fifo(&r_isd_registerValue, 1))
                    {
                        vtg_Lock_state = tdc_stim_data_extract_and_rshift(r_isd_registerValue, bitPosition_VTG_LOCK, 1);
#if 0
                        vtg_Lock_state=0;
#endif
                        if (vtg_Lock_state == 1)
                        {
                            // tdc_led_enable_test_trigger();
                            ///////////////////////////////////////
                            tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
                            //////////////////////////////////////

                            tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                            tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                            tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                            TDC_PRINTF_V("[ISD] SUCCESS TO ENABLE STIMULATION 10V \r\n");
                        }
                        else
                        {
                            // 설정 반복
                            flowControlCounter = -1;
                            iterationCouter++;
#if 1
                            if (iterationCouter > 10)
                            {
                                tdc_sys_error_update(en__EN__ISD_ERROR, en__VTG_Lock_Error, __LINE__);
                                tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                            }
#endif
                            TDC_PRINTF_W("[ISD] FAILED TO ENABLE STIMULATION 10V \r\n");
                        }
                    }
                }
                else
                {
                    tdc_isd_fpga_read_systemregister_1st(&r_FPGA_registerValue);
                    tdc_isd_fpga_read_backtel_error_flag(&r_FPGA_registerValue);

                    if (r_FPGA_registerValue == 0) // BACKETL 카운트가 1이 아니며, BACKETL 에러 레지스터의 값이 0이다.
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);
                        TDC_PRINTF_W("[ISD] FAILED TO ENABLE STIMULATION 10V (COUNT 0) \r\n");
                    }
                    else // BACKETL 에러 레지스터의 값이 0이 아니다.
                    {
                        tdc_sys_error_update(en__EN__ISD_ERROR, en__BackterDataLengthError, __LINE__);
                        TDC_PRINTF_W("[ISD] FAILED TO ENABLE STIMULATION 10V (BACKTEL ERROR) \r\n");
                    }

                    tdc_isd_change_state(en__isdStatus_FPGA_Ok); /// 내부기 전송 파워 설정 부터 다시.
                }
            }

#else
            ///////////////////////////////////////
            tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
            //////////////////////////////////////
#endif
        }
        break;
    }

    flowControlCounter++;
}
