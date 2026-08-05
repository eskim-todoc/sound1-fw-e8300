
#include <hw.h>
#include <stdbool.h>
#include <tdc_sys_error.h>
#include <tdc_ble_mapping.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_ble_mapping.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_impedance.h>
#include <tdc_isd_map_ecap.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd.h>
#include <tdc_isd_map_data.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd_init.h>
#include <FPGA.h>
#include <tdc_sys_control.h>
#include <tdc_ble_remote.h>
#include <tdc_ble_map_measure.h>
#include <tdc_ble_map_flash.h>
#include <tdc_ble_cmd_0x65_specific.h>
#include <tdc_ble_cmd_0x66_live.h>
#include <tdc_ble_reply.h>

static ST__MAPPING_PACKET mappingPacket;

// 데이터 인덱스 연속성 검사 카운터.
// 원래 fetch_packet 의 함수 지역 static 이었으나, 명령별 파싱 파일이 분리되면서
// 파일 경계를 넘게 되어 파일 스코프로 올리고 접근자로 노출한다.
static int prev_subCommandData_Num_index = 0;

int tdc_ble_mapping_get_seq_index(void)
{
    return prev_subCommandData_Num_index;
}

void tdc_ble_mapping_set_seq_index(int seqIndex)
{
    prev_subCommandData_Num_index = seqIndex;
}

void tdc_ble_mapping_clear_command()
{
    mappingPacket.command                    = en__mapping_IDLE;
    mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);
}

void tdc_ble_mapping_change_command_ble_disconnected(void)
{
    mappingPacket.command = en__mapping_ble_disconneted;
}

void tdc_ble_mapping_change_command_waiting_ble_off(void)
{
    mappingPacket.command = en__mapping_waiting_for_BleOff;
}

// const ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void)
ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void)
{
    return &mappingPacket;
}

void tdc_ble_mapping_fetch_packet(const uint8_t *Rx_dataPacket)  // spi 통신에서 호출 됨
{
    int index;
    int tempCommand;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index;

    bool exceptionCase = false;

    index           = 0;
    buffer_tx_index = 0;
    tempCommand     = Rx_dataPacket[index++];  // index 0 → 1

    // mappingPacket은 static 전역 구조체이다.
    // 그러므로 초기 값으로 mappingPacket.command는 en__mapping_IDLE로 시작한다.

    // 이전 명령이 완료되지 않아
    // mappingPacket.command가 en__mapping_IDLE가 아닌 상태에서
    // 새 명령이 입력되면
    // 아래와 같이 특정 예외 상황이 아니면 에러 응답을 송신한다.
    if (mappingPacket.command != en__mapping_IDLE)
    {
        if (tempCommand == en__mapping_live_stimulation)  // header 0x66 // 예외 사항
        {
            // 프로토콜 문서 상 0x66 명령어의 subCommand 1~8이 아닌,
            // 9: en__HoldOn, 0: en__Standby 인 경우 예외로 처리한다.
            if ((mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn) || (mappingPacket.tdc_isd_map_live_step.subCommand == en__Standby))
            {
                exceptionCase = true;
            }
        }

        if (tempCommand == en__mapping_disconnect)  // header 0x61
        {
            exceptionCase = true;
        }

        if (!exceptionCase)
        {
            // 현재 받은 명령을 수행하지 않는다.
            tdc_sys_error_send_to_app(tempCommand, en__EN__BLE_PROTOCOL_ERROR, en__PreviouCommnadIsNotCompleted, __LINE__);
            tempCommand = en__mapping_IDLE;
        }
    }

    switch (tempCommand)
    {
        case en__mapping_connect:  // header 0x60
        {
            mappingPacket.fetched_command = tempCommand;
            prev_subCommandData_Num_index = 0;
        }
        break;

        case en__mapping_disconnect:  // header 0x61
        {
            mappingPacket.fetched_command = tempCommand;
        }
        break;

        case en__mapping_impedanceChekck:  // 0x62
        {
            tdc_ble_cmd_0x62_impedance_check(Rx_dataPacket);
        }
        break;

        case en__mapping_eCAP_Measurement_masking:  // 헤더 0x063
        {
            tdc_ble_cmd_0x63_ecap_masking(Rx_dataPacket);
        }
        break;

        case en__mapping_eCAP_Measurement_alternative:  // 헤더 0x64
        {
            tdc_ble_cmd_0x64_ecap_alternative(Rx_dataPacket);
        }
        break;

        case en__mapping_specific_stimulation:  // 헤더 0x65
        {
            tdc_ble_cmd_0x65_specific_stim(Rx_dataPacket);
        }
        break;

        case en__mapping_live_stimulation:  // 헤더 0x66
        {
            tdc_ble_cmd_0x66_live(Rx_dataPacket);
        }
        break;

        case en__mapping_deviceStatus:  // 헤더 0x67 (외부기 상태)
        {
            mappingPacket.fetched_command = en__mapping_deviceStatus;
        }
        break;

        case en__mapping_read_original_ISD_N_USER:  // 헤더 0x69 (외부기 원래 내부기 정보 읽어 오기)
        {
            tdc_ble_cmd_0x69_read_original_isd_user();
        }
        break;

        case en__mapping_write_original_ISD_N_USER:  // 헤더 0x68 (외부기 최초연결 사용자 이름 등록)
        {
            tdc_ble_cmd_0x68_write_original_isd_user(Rx_dataPacket);
        }
        break;

        case en__mapping_read_SlotData_ISD_N_USER:  // 헤더 0x6A (맵 프로그램 관리: 읽기 - 내부기 ID 및 사용자)
        {
            tdc_ble_cmd_0x6A_read_slot_data(Rx_dataPacket);
        }
        break;

        case en__mapping_write_SlotData_ISD_N_USER:  // 헤더 0x6C (맵 프로그램 관리: 쓰기 - 내부기 ID 및 사용자)
        {
            tdc_ble_cmd_0x6C_write_slot_data(Rx_dataPacket);
        }
        break;

        case en__mapping_read_Mapdata_STIMUL_PARA:  // 헤더 0x6B (맵 프로그램 관리: 읽기 - 맵 데이터)
        {
            tdc_ble_cmd_0x6B_read_map_data(Rx_dataPacket);
        }
        break;

        case en__mapping_write_Mapdata_STIMUL_PARA:  // 헤더 0x6D (맵 프로그램 관리: 쓰기 - 맵 데이터)
        {
            tdc_ble_cmd_0x6D_write_map_data(Rx_dataPacket);
        }
        break;

        case en__mapping_erase_SlotData_manufacture:  // 헤더 0x6E (선택한 슬롯의 모든 맵데이터 삭제)
        {
            tdc_ble_cmd_0x6E_erase_slot(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_erase_mapData_STIMUL_PARA:  // 헤더 0x6F (선택한 맵 삭제)
        {
            tdc_ble_cmd_0x6F_erase_map(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_recover_mppingData_exceptSlot_1:  // 0x70
        {
            tdc_ble_cmd_0x70_recover_except_slot1(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_recover_ALL_SlotData_ManufactureData:  // 0x71
        {
            tdc_ble_cmd_0x71_recover_all(tempCommand);
        }
        break;

        case en__mapping_read_Connected_ISD_id:
        {
            mappingPacket.fetched_command = tempCommand;
        }
        break;

        default:
        {
            // command loop-back
            buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, tempCommand);

            buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, en__UndefinedCommand);  //

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
        }
        break;
    }
}

ST__MAPPING_STATE tdc_ble_mapping_step(ST__ISD_STATUS ISD_state)
{
    static EN__MAPPING_COMMAND prev_mppingCommand      = en__mapping_IDLE;
    static int                 connectionCheckCounter  = df_connectionCheckPeriod_ms;
    static int                 delayCounter            = 0;
    static bool                mappingProgramConnected = false;

    ST__MAPPING_STATE     mappingStatus     = {en__isdStatus_NA, false, false, false};
    EN__ISD_CONTROL_STATE isdControlCommand = en__isdStatus_NA;
    tdc_sys_error_code_t        errorCode;

    uint8_t  bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index;
    int  i;
    int  value;

    bool mappingCommandStartFlag = false;
    bool result;
    bool sitmulationIndicatorTrigger = false;
    bool BLE_Off_mapping             = false;

    // 매핑 명령이 수신되 시접에 내부기 연결 확인용 backtel 전송 명령이 실행 중일 경우에는 백텔 수신이 완료되고 명령을 실행 할 수 있도록 한다.
    if (mappingPacket.fetched_command != en__mapping_IDLE)
    {
        if (!tdc_isd_is_connection_check_with_mapping())
        {
            mappingPacket.command         = mappingPacket.fetched_command;
            mappingPacket.fetched_command = en__mapping_IDLE;

            // TDC_PRINTF_V("\r\n[MAPPING] COMMAND <-- FETCHED COMMAND (0x%02X) \r\n", mappingPacket.command);
        }
        else
        {
            if (prev_mppingCommand != mappingPacket.command)
            {
                TDC_PRINTF_V("\r\n[MAPPING] FETCHED COMMAND REMAINED : 0x%02X \r\n", mappingPacket.fetched_command);
                TDC_PRINTF_V("[MAPPING] PREV COMMAND : 0x%02X, CURRENT COMMAND : 0x%02X \r\n", prev_mppingCommand, mappingPacket.command);
                TDC_PRINTF_V("[MAPPING] BUT, NOW WAITING FOR LINK CONNECTION CHECK \r\n");
            }
        }
    }

    if (prev_mppingCommand != mappingPacket.command)
    {
        mappingCommandStartFlag = true;
    }
    else
    {
        mappingCommandStartFlag = false;
    }

    prev_mppingCommand = mappingPacket.command;

    buffer_tx_index = 0;

    if (mappingPacket.command == en__mapping_connect)
    {
        mappingProgramConnected = true;
        connectionCheckCounter  = 10;  // 연결되고 10msec 이후에 연결 체크(백텔)를 진행하도록 카운터 설정

        // 송신 데이터 준비
        buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, mappingPacket.command);  // command loop-back
        buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, 1);                      // pay-load 준비
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사
        mappingPacket.command = en__mapping_IDLE;                    // 명령 종료
    }
    else
    {
        if (mappingProgramConnected)
        {
            switch (mappingPacket.command)
            {
                case en__mapping_disconnect:
                {
                    // 송신 데이터 준비
                    buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, mappingPacket.command);  // command loop-back
                    buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, 1);                      // pay-load 준비
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사

#if 1
                    while (1)
                    {
                        // 매핑 앱에서 (블루투스가 아닌, 매핑 기능) 연결을 종료하는 경우,
                        // SPI TX 버퍼가 모두 전송되고 난 뒤 시스템을 재부팅 시킨다. (with 워치독 리셋)
                        if (tdc_hal_spi_is_tx_buffer_empty())
                        {
                            tdc_ble_mapping_clear_command();
                            tdc_shm_change_system_mode_flag(en__systemReset);

                            for (int i = 0; i < 100; i++)
                            {
                                SYS_WATCHDOG_REFRESH();
                                Sys_Delay((SystemCoreClock / 1000));  // 1ms
                            }

                            SYS_WATCHDOG_RESET();  // NOTE: 강제 리셋
                        }
                    }
#else
                    tdc_ble_mapping_clear_command();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // tdc_isd_update_link_disconnected();
#endif
                }
                break;

                case en__mapping_ble_disconneted:
                {
                    tdc_ble_mapping_clear_command();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // tdc_isd_update_link_disconnected();
                }
                break;

                case en__mapping_impedanceChekck:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;

                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_impedance_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        TDC_PRINTF_E("[MAPPING] CAN NOT CHECK IMPEDANCE, BECAUSE ISD NOT CONNECTED \r\n");
                        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_masking:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_ecap_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_alternative:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    // tdc_isd_map_ecap_step(mappingCommandStartFlag);
                }
                break;

                case en__mapping_specific_stimulation:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_specific_stim_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_live_stimulation:
                {
                    connectionCheckCounter      = df_connectionCheckPeriod_ms;
                    sitmulationIndicatorTrigger = tdc_isd_map_live_step(ISD_state);
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                }
                break;

                case en__mapping_deviceStatus:
                {
                    // command loop-back
                    buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__mapping_deviceStatus);

                    // 내부기 연결 상태
                    if (ISD_state.conneded_ISD)
                    {
                        buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, 1);  // 연결됨
                    }
                    else
                    {
                        buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, 2);  // 끊어짐
                    }

                    // 배터리 레벨
                    buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, tdc_pwr_battery_read_percentage());

                    // tdc_sys_error_code_t 전달
                    errorCode = tdc_sys_error_read();

                    buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, (int) errorCode.ISD_ErrorFlag);

                    buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, 0);

                    // NRF에 전달

                    // 송신 데이터 SPI TX버퍼에 복사
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                    tdc_ble_mapping_clear_command();
                }
                break;

                case en__mapping_read_original_ISD_N_USER:
                {
                    tdc_isd_map_read_original_info_setting(mappingCommandStartFlag, en__mapping_read_original_ISD_N_USER);
                }
                break;

                case en__mapping_write_original_ISD_N_USER:
                {
                    /* 플레쉬에 저장하면서 BLE 광고 이름을 바꾸기 위해서
                     * tdc_isd_change_state(en__isdStatus_ISD_Power_Ok);를 사용해 내부기 연결 해제를 유도하여
                     * BLE 연결 해제 후 광고이름 변경하여 진행하게 한다. */
                    tdc_isd_map_write_original_info_setting(mappingCommandStartFlag, en__mapping_write_original_ISD_N_USER);
                }
                break;

                case en__mapping_read_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_read_info_setting(mappingCommandStartFlag, en__mapping_read_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_write_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_write_info_setting(mappingCommandStartFlag, en__mapping_write_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_read_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_read_stim_para(mappingCommandStartFlag, en__mapping_read_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_write_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_write_stim_para(mappingCommandStartFlag, en__mapping_write_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_erase_SlotData_manufacture:
                {
                    tdc_isd_map_reset_nvm_selected(mappingCommandStartFlag, en__mapping_erase_SlotData_manufacture, mappingPacket.ReadWriteMapData_Flash.slot_index, flash_Command_Erase);
                }
                break;

                case en__mapping_erase_mapData_STIMUL_PARA:
                {
                    tdc_isd_map_reset_nvm_map_data(mappingCommandStartFlag, en__mapping_erase_mapData_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index, flash_Command_Erase);
                }
                break;

                case en__mapping_recover_mppingData_exceptSlot_1:
                {
                    tdc_isd_map_reset_nvm_2to4(mappingCommandStartFlag, en__mapping_recover_mppingData_exceptSlot_1, flash_Command_Recover);
                }
                break;

                case en__mapping_recover_ALL_SlotData_ManufactureData:
                {
                    result = tdc_isd_map_reset_nvm_all(mappingCommandStartFlag, en__mapping_recover_ALL_SlotData_ManufactureData, flash_Command_Recover);

                    if (result)
                    {
                        while (1)
                        {
                            if (tdc_hal_spi_is_tx_buffer_empty())
                            {
                                if (en__mapping_recover_ALL_SlotData_ManufactureData > 0x60)
                                {
                                    tdc_ble_mapping_clear_command();
                                }
                                else
                                {
                                    tdc_ble_remote_clear_command();
                                }

                                tdc_shm_change_system_mode_flag(en__systemReset);

                                // NOTE: 강제 리셋

                                for (int i = 0; i < 100; i++)
                                {
                                    SYS_WATCHDOG_REFRESH();
                                    Sys_Delay((SystemCoreClock / 1000));
                                }

                                SYS_WATCHDOG_RESET();
                            }
                        }
                    }
                }
                break;

                case en__mapping_waiting_for_BleOff:
                {
                    delayCounter++;

                    if (delayCounter == 150)  // 수신명령 응답 돤료 이후에 NRF끔
                    {
                        // tdc_isd_update_link_disconnected();
                        isdControlCommand = en__isdStatus_PowerIC_Reset;
                    }

                    if (delayCounter > 150)
                    {
                        // 내부기가 연결된 상태에서 BLE를 다시 켜야 광고이름이 제대로 나올 수 있음.
                        if (ISD_state.isd_controlState < en__isdStatus_ISD_pathOpen_Ok)
                        {
                            BLE_Off_mapping = true;
                        }
                        else
                        {
                            tdc_ble_mapping_clear_command();
                            delayCounter = 0;
                        }
                    }
                }
                break;

                case en__mapping_read_Connected_ISD_id:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, mappingPacket.command);

                    // pay-load 준비
                    value                              = tdc_isd_read_connected_id();
                    buffer_tx_index = tdc_ble_reply_u32(bufferForSPI_tx, buffer_tx_index, value);

                    // 송신 데이터 SPI TX버퍼에 복사
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                    //  명령 종료
                    tdc_ble_mapping_clear_command();
                }
                break;

                case en__mapping_IDLE:
                default:
                {
                    if (ISD_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)  // 내부기 초기화가 완로되어야 연결 확인이 가능하다.
                    {
                        /* 링크체크 진입 로그(connectionCheckCounter == 0 일 때 1회 출력)는
                         * 2차 리팩토링에서 제거했다(#if 0 사장). 단순 디버그 출력이었다. */
                        tdc_isd_update_link_by_backtel_mapping(connectionCheckCounter);  // 체크가 완료되면 flag가 FLASE로 변경
                    }
                }
                break;
            }

            connectionCheckCounter--;

            if (connectionCheckCounter < 0)
            {
                connectionCheckCounter = df_connectionCheckPeriod_ms;
            }
        }
        else
        {
            if (mappingPacket.command > en__mapping_connect)
            {
                tdc_sys_error_send_to_app(mappingPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);
                tdc_ble_mapping_clear_command();
            }
        }
    }

    mappingStatus.StimulationIndicatorTrigger = sitmulationIndicatorTrigger;
    mappingStatus.BLE_Off                     = BLE_Off_mapping;
    mappingStatus.mappingConnection           = mappingProgramConnected;
    mappingStatus.isdControlCommand           = isdControlCommand;

    return mappingStatus;
}

/* import_*ForDebug / import_live* 계열 15개 제거(2026-07-22, G6).
 * mapping 프로토콜 수동 주입용 구 디버그 진입점. 정의는 #if 0 블록(396줄) 안에서
 * 죽어 있었고 헤더 선언 16개도 호출처가 0 이었다(import_livePause 는 선언만 있고
 * 정의조차 없는 고아였다). 상세: docs/tasks/cm3/20260720_cm3-full-refactor/게이트-노트/G6-ble-qcc.md */




















