#include <hw.h>
#include <stdbool.h>

#include <tdc_sys_error.h>
#include <tdc_shm.h>
#include <tdc_ble_mapping.h>
#include <tdc_ble_remote.h>
#include <tdc_hal_spi.h>
#include <tdc_isd_map_data.h>

void tdc_isd_map_read_original_info_setting(bool startFlag, int command)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;
    int                                               *p_RepositoryFor_ISD_info;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    int        i;
    static int prevPcmOutputMode;
    static int dataPacket_index;

    buffer_tx_index = 0;

    if (startFlag)
    {

        FlashCommand.flashCommand = flash_Command_Read;
        FlashCommand.isd_index    = 1;
        FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 읽도록 한다.

        dataPacket_index = 1;

        prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
    }
    else
    {
        if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 읽기가 완료된 상태
        {

            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            p_RepositoryFor_ISD_info = tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info();

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            switch (dataPacket_index)
            {
                case 1:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 0; i < 18; i++)
                    {
                        switch (i)
                        {
                            case 0:
                            case 1:
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i];  // 내부기 제조번호 (년, 월_모델번호)
                                break;
                            case 2:                                                                     // 시리얼 번호 상위 8bit
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i] >> 8;  // 시리얼 번호 상위 8bit

                                break;
                            case 3:                                                                           // 시리얼 번호 하위 8bit
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1] & 0xFF;  // 시리얼 번호 상위 8bit

                                break;

                            default:
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];
                                break;
                        }
                    }
                }
                break;

                case 2:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 18; i < 30; i++)
                    {
                        // 내부기 정보 -- 사용자 이름 나머지, 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];
                    }
                }
                break;
            }

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            if (dataPacket_index == numPacket_readMapData_Original_ISDnSetting)  // 명령 완료
            {
                if (command > 0x60)
                {
                    tdc_ble_mapping_clear_command();
                }
                else
                {
                    tdc_ble_remote_clear_command();
                }
            }

            dataPacket_index++;
        }
    }
}

void tdc_isd_map_write_original_info_setting(bool startFlag, int command)
{
    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t         bufferForSPI_tx[BLE_DataPacketSize];
    int         buffer_tx_index;
    static bool prev_ISD_id_match = false;
    static bool startFlashCommand = false;

    static int counter = 0;

    ST__MAPPING_PACKET *p_mappingPacket;

    buffer_tx_index = 0;

    if (startFlag)
    {
        tdc_isd_change_state(en__isdStatus_ISD_Power_Ok);
        counter = 0;
        // 내부기 id 확인을 시작한다.
    }

    p_mappingPacket = tdc_ble_mapping_get_packet();

    if (p_mappingPacket->rx_orignal_ISD_info.id_check_is_completed)
    {

        if (p_mappingPacket->rx_orignal_ISD_info.id_match)
        {

            if (!prev_ISD_id_match)
            {
                // 플레쉬에 저장하면서 NRF 광고 이름을 바꾸기 위해서 NRF를 껏다가 켠다. 이러한 이유로 NRF를 끄기전에 수신된 명령을 루프백한다.
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = command;

                // payload num 전송

                bufferForSPI_tx[buffer_tx_index++] = 2;  // 정상

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
            }

            if (tdc_hal_spi_is_tx_buffer_empty())  // NRF로 응답 명령의 전송이 완료된 상태.
            {

                // 1회 동작
                if (!startFlashCommand)
                {
                    FlashCommand.flashCommand = flash_Command_Write;
                    FlashCommand.isd_index    = 1;
                    FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

                    /* PCM 모드를 저장해 두지 않는다. 이 함수는 외부기 맵 데이터가 공장 초기화된 상태에서
                     * 최초 연결 시 사용자 이름을 등록하려고 딱 한 번 쓰는 경로다. 매핑 연결 후에는
                     * 라이브모드가 아닌 한 NopStandby 가 기본 대기 상태이므로 복원이 필요 없다.
                     * (형제 함수들은 라이브 중에도 불릴 수 있어 저장·복원을 한다. 2026-07-27 은수님 확인) */
                    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                    tdc_shm_set_read_write_map_data_flash_command(FlashCommand);

                    startFlashCommand = true;
                }

                if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
                {

                    startFlashCommand = false;
                    tdc_ble_mapping_change_command_waiting_ble_off();
                }
            }
        }
        else
        {

            // id가 맞지 않는다.
            tdc_sys_error_send_to_app(command, en__EN__ISD_ERROR, en__No_Matched_ISD_ID, __LINE__);

            // 커맨드 리셋;
            tdc_ble_mapping_clear_command();
        }
    }

    if (counter > 2000)
    {

        // id가 0으로 읽혀서 계속 시도 함으으로써 시간 초과
        tdc_sys_error_send_to_app(command, en__EN__ISD_ERROR, en__ISD_EEPROM_ValueZero, __LINE__);

        // 커맨드 리셋;
        tdc_ble_mapping_clear_command();

        counter = 0;
    }

    prev_ISD_id_match = p_mappingPacket->rx_orignal_ISD_info.id_match;
    counter++;
}

void tdc_isd_map_read_info_setting(bool startFlag, int command, int slot_index)
{
    static int prevPcmOutputMode;
    static int dataPacket_index;

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;
    int                                               *p_RepositoryFor_ISD_info;
    uint8_t                                                bufferForSPI_tx[BLE_DataPacketSize];
    int                                                buffer_tx_index;
    int                                                i;

    buffer_tx_index = 0;

    if (startFlag)
    {
        FlashCommand.flashCommand = flash_Command_Read;
        FlashCommand.isd_index    = slot_index;
        FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 읽도록 한다.

        dataPacket_index = 1;

        prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
    }
    else
    {
        if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 읽기가 완료된 상태
        {
            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            p_RepositoryFor_ISD_info = tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info();

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            switch (dataPacket_index)
            {
                case 1:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 0; i < 18; i++)
                    {
                        switch (i)
                        {
                            case 0:
                            case 1:
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i];  // 내부기 제조번호 (년, 월_모델번호)
                                break;

                            case 2:                                                                     // 시리얼 번호 상위 8bit
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i] >> 8;  // 시리얼 번호 상위 8bit
                                break;

                            case 3:                                                                           // 시리얼 번호 하위 8bit
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1] & 0xFF;  // 시리얼 번호 상위 8bit
                                break;

                            default:
                                bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];
                                break;
                        }
                    }
                }
                break;

                case 2:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 18; i < 36; i++)
                    {
                        // 내부기 정보 -- 사용자 이름 나머지, 설정값(패스 key, 맵번호, 자극 볼륨)
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];  //  시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                    }
                }
                break;

                case 3:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 36; i < 40; i++)
                    {
                        // 설정값(오디오 볼륨, LED, 자극알림, 텔레코일), ==> ble on off는 전달하지 않는다.
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];  //  시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                    }

                    for (i = 41; i < 47; i++)
                    {
                        // 맵 스템프 6
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_ISD_info[i - 1];  // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                    }
                }
                break;
            }

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            if (dataPacket_index == numPacket_readMapData_ISDnSetting)  // 명령 완료
            {
                // 명령 종료
                if (command > 0x60)
                {
                    tdc_ble_mapping_clear_command();
                }
                else
                {
                    tdc_ble_remote_clear_command();
                }
            }

            dataPacket_index++;
        }
    }
}

void tdc_isd_map_write_info_setting(bool startFlag, int command, int slot_index)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    int       *p_RepositoryFor_buffer;
    static int prevPcmOutputMode;

    buffer_tx_index = 0;

    if (startFlag)
    {

        FlashCommand.flashCommand = flash_Command_Write;
        FlashCommand.isd_index    = slot_index;
        FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

        prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
    }
    else
    {
        if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
        {

#if 0

                                        p_RepositoryFor_buffer= tdc_shm_get_pointer_repository_for_read_write_map_data_user_setting();


                                        tdc_shm_change_program_map_num(p_RepositoryFor_buffer[0]);
                                        tdc_shm_change_stimul_volume(p_RepositoryFor_buffer[1]);
                                        tdc_shm_change_audio_volume(p_RepositoryFor_buffer[2]);
                                        tdc_shm_change_led_indicator_on_off(p_RepositoryFor_buffer[3]);
                                        tdc_shm_change_stimul_indicator_on_off(p_RepositoryFor_buffer[4]);
                                        tdc_shm_change_tele_coil_on_off(p_RepositoryFor_buffer[5]);

#endif

            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            // 마지막으로 수신된 명령을 루프백 한다.

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            // payload num 전송

            bufferForSPI_tx[buffer_tx_index++] = numPacket_writeMapData_ISDnSetting;  // payload num 전송

            // NRF에 전달

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 명령 종료
            if (command > 0x60)
            {
                tdc_ble_mapping_clear_command();
            }
            else
            {
                tdc_ble_remote_clear_command();
            }
        }
    }
}

void tdc_isd_map_read_stim_para(bool startFlag, int command, int slot_index, int map_index)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;
    int                                               *p_RepositoryFor_stimulPara;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    int        i;
    int        byteFromInt;
    static int stimulPara_index = 0;
    static int prevPcmOutputMode;
    static int dataPacket_index;

    buffer_tx_index = 0;

    if (startFlag)
    {

        FlashCommand.flashCommand = flash_Command_Read;
        FlashCommand.isd_index    = slot_index;
        FlashCommand.map_index    = map_index;

        dataPacket_index = 1;

        prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        tdc_shm_set_read_write_map_data_flash_command(FlashCommand);

        stimulPara_index = 0;
    }
    else
    {
        if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 읽기가 완료된 상태
        {

            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            p_RepositoryFor_stimulPara = tdc_shm_get_pointer_repository_for_read_write_map_data_stimul_para();

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            switch (dataPacket_index)
            {
                case 1:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 0; i < 12; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  // 매핑 일자, 자극 펄스 파라미터 들..
                    }

                    byteFromInt = p_RepositoryFor_stimulPara[stimulPara_index++];

                    bufferForSPI_tx[buffer_tx_index++] = (byteFromInt >> 8);    // 알림자극 크기 상위바이트
                    bufferForSPI_tx[buffer_tx_index++] = (byteFromInt & 0xFF);  // 알림자극 크기 하위
                }
                break;

                case 2:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 자극 전극  17개,
                    for (i = 0; i < 17; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;

                case 3:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 자극 전극 15개
                    for (i = 0; i < 15; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;

                case 4:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 바이폴라 기준전극  17개
                    for (i = 0; i < 17; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;
                case 5:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 바이폴라 전극  15개
                    for (i = 0; i < 15; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;
                case 6:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 주파수 밴드 출력 순서  기준전극  17개
                    for (i = 0; i < 17; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;
                case 7:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;  // 주파수 밴드 출력 순서  기준전극  15개
                    for (i = 0; i < 15; i++)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = p_RepositoryFor_stimulPara[stimulPara_index++];  //
                    }
                }
                break;

                case 8:
                case 9:
                case 10:
                case 11:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 0; i < 8; i++)
                    {
                        byteFromInt = p_RepositoryFor_stimulPara[stimulPara_index++];

                        bufferForSPI_tx[buffer_tx_index++] = (byteFromInt >> 8);    // T_level 크기 상위바이트
                        bufferForSPI_tx[buffer_tx_index++] = (byteFromInt & 0xFF);  // T_level 크기 하위바이트
                    }
                }
                break;

                case 12:
                case 13:
                case 14:
                case 15:
                {
                    bufferForSPI_tx[buffer_tx_index++] = dataPacket_index;
                    for (i = 0; i < 8; i++)
                    {
                        byteFromInt = p_RepositoryFor_stimulPara[stimulPara_index++];

                        bufferForSPI_tx[buffer_tx_index++] = (byteFromInt >> 8);    // C_level 크기 상위바이트
                        bufferForSPI_tx[buffer_tx_index++] = (byteFromInt & 0xFF);  // C_level 크기 하위바이트
                    }
                }
                break;
            }

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            if (dataPacket_index == numPacket_readMapData_stimulPara)  // 명령 완료
            {

                stimulPara_index = 0;

                // 명령 종료
                if (command > 0x60)
                {
                    tdc_ble_mapping_clear_command();
                }
                else
                {
                    tdc_ble_remote_clear_command();
                }
            }

            dataPacket_index++;
        }
    }
}

void tdc_isd_map_write_stim_para(bool startFlag, int command, int slot_index, int map_index)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    static int prevPcmOutputMode;

    buffer_tx_index = 0;

    if (startFlag)
    {

        FlashCommand.flashCommand = flash_Command_Write;
        FlashCommand.isd_index    = slot_index;
        FlashCommand.map_index    = map_index;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

        prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
    }
    else
    {
        if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
        {

            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            // 마지막으로 수신된 명령을 루프백 한다.

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            // payload num 전송

            bufferForSPI_tx[buffer_tx_index++] = numPacket_writeMapData_stimulPara;  // payload num 전송

            // NRF에 전달

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 명령 종료
            if (command > 0x60)
            {
                tdc_ble_mapping_clear_command();
            }
            else
            {
                tdc_ble_remote_clear_command();
            }
        }
    }
}

// 0x71

void tdc_isd_map_reset_nvm_selected(bool startFlag, int command, int slot_index, EN__mapping_ReadWriteMap_command RecoverOrErase)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    static int prevPcmOutputMode;

    if ((RecoverOrErase == flash_Command_Erase) || (RecoverOrErase == flash_Command_Recover))
    {
        buffer_tx_index = 0;

        if (startFlag)
        {

            FlashCommand.flashCommand = RecoverOrErase;
            FlashCommand.isd_index    = slot_index;
            FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

            prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

            tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
        }
        else
        {
            if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
            {

                tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

                // 마지막으로 수신된 명령을 루프백 한다.

                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = command;

                // payload num 전송

                bufferForSPI_tx[buffer_tx_index++] = slot_index;  // 삭제된 초기화된 slot 번호

                // NRF에 전달

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 명령 종료
                if (command > 0x60)
                {
                    tdc_ble_mapping_clear_command();
                }
                else
                {
                    tdc_ble_remote_clear_command();
                }
            }
        }
    }
    else
    {

        // 에러 전송
        tdc_sys_error_send_to_app(command, en__dataProcessing_ERROR, en_sourceCodeError, __LINE__);  // 소스 코드 에러

        // 명령 종료
        if (command > 0x60)
        {
            tdc_ble_mapping_clear_command();
        }
        else
        {
            tdc_ble_remote_clear_command();
        }
    }
}

// 0x 57
bool tdc_isd_map_reset_nvm_all(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase)
{
    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    static int prevPcmOutputMode;
    static int counter          = 0;
    static int slot_index       = 0;
    bool       CommandCompleted = false;

    buffer_tx_index = 0;

    if ((RecoverOrErase == flash_Command_Erase) || (RecoverOrErase == flash_Command_Recover))
    {
        if (startFlag)
        {
            TDC_PRINTF_I("[FLASH] RECOVER OR ERASE PACKET, START FLAG : TRUE \r\n");

            counter          = 0;
            slot_index       = 1;
            CommandCompleted = false;
        }

        switch (counter)
        {
            case 0:
                FlashCommand.flashCommand = RecoverOrErase;
                FlashCommand.isd_index    = slot_index;
                FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

                prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                TDC_PRINTF_I("[FLASH] COMMAND (%d), ISD INDEX (%d), MAP INDEX (%d) \r\n", FlashCommand.flashCommand, FlashCommand.isd_index, FlashCommand.map_index);
                tdc_shm_set_read_write_map_data_flash_command(FlashCommand);

                break;

            default:
                if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
                {
                    slot_index++;
                    counter = -1;

                    TDC_PRINTF_I("[FLASH] DONE FOR READ/WRITE MAPDATA FLASH COMMAND, SLOT INDEX (%u), COUNTER (%d) \r\n", slot_index, counter);
                }
                break;
        }

        if (slot_index == 5)
        {
            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            // 마지막으로 수신된 명령을 루프백 한다.

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            // payload num 전송

            bufferForSPI_tx[buffer_tx_index++] = en__DONE_OK;
            // NRF에 전달

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 명령 종료
            CommandCompleted = true;
        }

        counter++;
    }
    else
    {
        // 에러 전송
        tdc_sys_error_send_to_app(command, en__dataProcessing_ERROR, en_sourceCodeError, __LINE__);  // 소스 코드 에러
    }

    return CommandCompleted;
}

void tdc_isd_map_reset_nvm_2to4(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    static int prevPcmOutputMode;
    static int counter    = 0;
    static int slot_index = 0;

    if ((RecoverOrErase == flash_Command_Erase) || (RecoverOrErase == flash_Command_Recover))
    {
        buffer_tx_index = 0;

        if (startFlag)
        {

            counter          = 0;
            slot_index       = 1;
        }

        switch (counter)
        {
            case 0:
                FlashCommand.flashCommand = flash_Command_Recover;
                FlashCommand.isd_index    = slot_index;
                FlashCommand.map_index    = 0;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

                prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                tdc_shm_set_read_write_map_data_flash_command(FlashCommand);

                break;

            default:

                if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
                {
                    slot_index++;
                    counter = -1;
                }

                break;
        }

        if (slot_index == 4)
        {

            tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

            // 마지막으로 수신된 명령을 루프백 한다.

            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = command;

            // payload num 전송

            bufferForSPI_tx[buffer_tx_index++] = en__DONE_OK;  // 삭제된 초기화된 slot 번호

            // NRF에 전달

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 명령 종료
            if (command > 0x60)
            {
                tdc_ble_mapping_clear_command();
            }
            else
            {
                tdc_ble_remote_clear_command();
            }
        }

        counter++;
    }
    else
    {
        // 에러 전송
        tdc_sys_error_send_to_app(command, en__dataProcessing_ERROR, en_sourceCodeError, __LINE__);  // 소스 코드 에러

        // 명령 종료
        if (command > 0x60)
        {
            tdc_ble_mapping_clear_command();
        }
        else
        {
            tdc_ble_remote_clear_command();
        }
    }
}

void tdc_isd_map_reset_nvm_map_data(bool startFlag, int command, int slot_index, int map_index, EN__mapping_ReadWriteMap_command RecoverOrErase)
{

    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash FlashCommand;

    uint8_t        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;
    static int prevPcmOutputMode;

    if ((RecoverOrErase == flash_Command_Erase) || (RecoverOrErase == flash_Command_Recover))
    {
        buffer_tx_index = 0;

        if (startFlag)
        {

            FlashCommand.flashCommand = RecoverOrErase;
            FlashCommand.isd_index    = slot_index;
            FlashCommand.map_index    = map_index;  // 맵 인덱스가 0이면 CFX에서 ISD 정보 및 사용자 설정값을 쓰도록 한다.

            prevPcmOutputMode = tdc_shm_read_current_pcm_output_mode();
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

            tdc_shm_set_read_write_map_data_flash_command(FlashCommand);
        }
        else
        {
            if (tdc_shm_is_read_write_map_data_flash_command_done())  // CFX에서 eeprom 쓰기가 완료된 상태
            {

                tdc_shm_change_pcm_output_mode(prevPcmOutputMode);

                // 마지막으로 수신된 명령을 루프백 한다.

                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = command;

                // payload num 전송

                bufferForSPI_tx[buffer_tx_index++] = en__DONE_OK; // 정상

                // NRF에 전달

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 명령 종료
                if (command > 0x60)
                {
                    tdc_ble_mapping_clear_command();
                }
                else
                {
                    tdc_ble_remote_clear_command();
                }
            }
        }
    }
    else
    {
        // 에러 전송
        tdc_sys_error_send_to_app(command, en__dataProcessing_ERROR, en_sourceCodeError, __LINE__); // 소스 코드 에러

        // 명령 종료
        if (command > 0x60)
        {
            tdc_ble_mapping_clear_command();
        }
        else
        {
            tdc_ble_remote_clear_command();
        }
    }
}
