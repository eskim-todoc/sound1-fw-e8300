

#include <stdbool.h>
#include "driver_SPI.h"
#include "isd_interface.h"
#include "remoteControl.h"
#include "mappingControl.h"
#include "ble_communication.h"

#include <batteryNPowerControl.h>

#include <ci_ble_control_boot.h>
#include <ci_ble_control_ota.h>

typedef struct
{
    ReadCommandForBleSetting command;
    int                      data[todoc_PayloadSize];

} BleSettingPacket;

BleSettingPacket bleSettingPacket;

void fetch_readDataForBleSetting(const int *Rx_dataPacket)
{
    bleSettingPacket.command = Rx_dataPacket[0];

    // NOTE: Sound1에서 추가된 QCC와의 특수 명령에 대해서,
    // 헤더 외에 데이터가 있는 특수 패킷의 경우 데이터를 복사해야 한다.
    // 현재는 0x34, Power info만 데이터가 존재하는 상태이지만,
    // 추후에 얼마나 명령어가 늘어날지 예측할 수 없다. (2026.03.12)

    if ((EN__SND_BT_CMD_SYSTEM_INFO_POWER <= Rx_dataPacket[0])        // 0x34 POWER INFO 부터
        && (Rx_dataPacket[0] <= EN__SND_BT_CMD_SYSTEM_INFO_LED_IND))  // 0x35 LED INDICATION 까지
    {
        for (int i = 0; i < todoc_PayloadSize; i++)
        {
            bleSettingPacket.data[i] = Rx_dataPacket[1 + i];
        }
    }
}

void setting_nrf_ble_adv_info(void)
{

    int Tx_dataBuff[BLE_DataPacketSize];

    int  connectedISD_num;
    int *p_currentUserName;

    int tx_index = 0;
    int i;

    if (bleSettingPacket.command == en__bleSetting_ReadConnected_ISD_info)
    {
        ST__ISD_STATUS isd_status;

        isd_status = snd_isd_interface_get_state();

        // ISD가 연결된 상태라면 내부기 정보 전달
        if (isd_status.conneded_ISD)
        {
            // 수술위치
            connectedISD_num        = read_connected_ISD_Num();
            Tx_dataBuff[tx_index++] = (int) readConnected_ISD_Location(connectedISD_num);

            // 사용자 이름
            p_currentUserName = readConnected_ISD_userName(connectedISD_num);
            for (i = 0; i < 10; i++)
            {
                Tx_dataBuff[tx_index++] = p_currentUserName[i];
            }
        }
        // ISD가 연결되지 않은 상태라면 0으로 채운 더미 데이터 전달
        // TX 크기가 0이면 SPI TX 버퍼에서 알아서 21바이트를 0으로 채워서 전달
        // 전송 크기가 0이므로 전송 바이트 수를 알려주는 마지막 바이트
        // 즉, [20] 인덱스도 0으로 채워져서 보내질 것이다.
        else
        {
            ci_printw("[BT] ISD NOT CONNECTED, BUT RESPONSE 0x30 COMMAND \r\n");
            tx_index = 0;
        }

        writeDataToSpiTxBuff(Tx_dataBuff, tx_index);     // 송신 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  //  명령 종료
    }
    // QCC와 새로 추가한 패킷 (0x33. Battery 정보)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_BATTERY)
    {
        int batt_percent;
        int charger_state;

        // 배터리 레벨을 QCC에게 수신한 이후로만 0xFF가 아닌 값을 전송한다.
        // 사실상 QCC가 배터리 레벨을 측정하기로 한 뒤로 쓸모가 없는 명령이 되었다.
        if (snd_batt_get_state() != EN__SND_BATT_STATE_RESET)
        {
            batt_percent = snd_batt_get_percent();
            ci_printd("[BT] READ BATT LEVEL, %d PERCENT \r\n", batt_percent);
        }
        else
        {
            batt_percent = 0xFF;
            ci_printd("[BT] READ BATT LEVEL NOT YET READY \r\n");
        }

        charger_state = snd_charger_get_state().chargerConnectorPluggedIn;

        Tx_dataBuff[tx_index++] = EN__SND_BT_CMD_SYSTEM_INFO_BATTERY;
        Tx_dataBuff[tx_index++] = batt_percent;
        Tx_dataBuff[tx_index++] = charger_state;  // 0: RESET, 1: CONNECTED, 2: DISCONNECTED

        writeDataToSpiTxBuff(Tx_dataBuff, tx_index);     // 송싱 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // QCC와 새로 초가한 패킷 (0x34, Power info)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_POWER)
    {
        int battery_level;      // 패킷 인덱스 1 → 헤더 제외 시, 데이터 인덱스 0
        int charger_connected;  // 패킷 인덱스 2 → 헤더 제외 시, 데이터 인덱스 1

        battery_level     = bleSettingPacket.data[0];  // 배터리 레벨
        charger_connected = bleSettingPacket.data[1];  // 충전기 연결 상태

        // 수신한 배터리 정보로 업데이트 한다.
        snd_batt_set_percent(battery_level);

        // 배터리 충전 상태인지 방전 즉, 일반 동작 상태인지는
        // 충전기 연결 상태에 따라서 배터리 상태 업데이트를 진행해야 한다.

        switch (charger_connected)
        {
            case 0:  // Disconnected
                snd_charger_set_state(EN__SND_CHARGER_STATE_DISCONNECTED);
                snd_batt_set_state(EN__SND_BATT_STATE_DISCHARGING);
                break;

            case 1:  // Connected
                snd_charger_set_state(EN__SND_CHARGER_STATE_CONNECTED);
                snd_batt_set_state(EN__SND_BATT_STATE_CHARGING);
                break;

            default:
                ci_printe("[BT] CMD 0x%02X, INVALID CHARGER CONNECTED: %d \r\n", EN__SND_BT_CMD_SYSTEM_INFO_POWER, charger_connected);
                snd_charger_set_state(EN__SND_CHARGER_STATE_RESET);
                snd_batt_set_state(EN__SND_BATT_STATE_RESET);
                break;
        }  // 끝, switch

        ci_printv("[BT] CMD 0x%02X, CHARGER STATE: %d, BATT LEVEL %d PERCENT \r\n", EN__SND_BT_CMD_SYSTEM_INFO_POWER, charger_connected, battery_level);

        Tx_dataBuff[tx_index++] = EN__SND_BT_CMD_SYSTEM_INFO_POWER;
        Tx_dataBuff[tx_index++] = 1;  // 수신 확인 응답

        writeDataToSpiTxBuff(Tx_dataBuff, tx_index);     // 송싱 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // 끝, else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_POWER)
    // 시작, QCC와 새로 초가한 패킷 (0x35, LED Indication)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_LED_IND)
    {
        int led_ind;  // 패킷 인덱스 1 → 헤더 제외 시, 데이터 인덱스 0

        /* 수신 패킷 파싱 */
        led_ind = bleSettingPacket.data[0];  // LED 표시 상태

        /* 명령 처리 */

        tdc_led_set_ind_state((tdc_led_ind_state_t) led_ind);  // Arbiter에 LED 표시 상태 반영 (Rev.3)

        ci_printv("[BT] CMD 0x%02X, LED IND: %d \r\n", EN__SND_BT_CMD_SYSTEM_INFO_LED_IND, led_ind);

        /* 응답 패킷 */
        Tx_dataBuff[tx_index++] = EN__SND_BT_CMD_SYSTEM_INFO_LED_IND;
        Tx_dataBuff[tx_index++] = 1;                     // 수신 확인 응답
        writeDataToSpiTxBuff(Tx_dataBuff, tx_index);     // 송신 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // 끝, else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_LED_IND)
}

ST__BLE_COMMUNICATION_STATE bleCommunication(ST__ISD_STATUS isd_state)
{
    int         i;
    static int  Rx_counter = 1;
    static int *p_Rx_dataPacket;

    EN__SPI_COMMU_STATE         communicationState;
    ST__MAPPING_STATE           mappingState;
    ST__BLE_COMMUNICATION_STATE ble_communication_state = {en__isdStatus_NA, false, false, false};
    ST__REMOTECONTROL_STATE     remoteControlState      = {en__isdStatus_NA, false};

    communicationState = get_spi_commu_state();

    // SPI 통신으로 nRF로부터 패킷을 수신하면 communicationState가 SPI_CMMM_FETCH로 업데이트 됨
    // SPI 통신에 이슈가 발생한 경우 SPI_CMMM_ERROR로 업데이트 됨
    switch (communicationState)
    {
        case SPI_COMM_IDLE:
        {
            i++;
        }
        break;

        case SPI_CMMM_FETCH:
        {
#if 0
            // NRF에서 수시된 데이터를 그대로 루프백하고, 마지막 데이터에 수신된 횟수를 추가하여 보낸다.
            for (i = 0; i < SPI_COMM_PACKET_SIZE; i++)
            {
                SPI_Tx_Buffer[i] = 0;
            }

            SPI->TX_DATA = SPI_Rx_Buffer[0];  // 첫번째 byte는 먼저 준비해 놓아야 된다.

            for (i = 0; i < SPI_COMM_PACKET_SIZE - 1; i++)
            {
                if (SPI_Rx_Buffer[i + 1] != 0)
                {
                    SPI_Tx_Buffer[i] = SPI_Rx_Buffer[i + 1];
                }
                else
                {
                    break;
                }
            }

            SPI_Tx_Buffer[i] = Rx_counter;

            set_spi_commu_state_IDLE();
            Rx_counter++;
#else
            p_Rx_dataPacket = getAddr_SPI_Rx_DataPacket();

#if 1  // nRF SPI 디버깅

            bool print_allowed = true;

            // 라이브모드의 실시간 전류 값을 제외하고 출력 (데이터 양이 너무 많음)
            if ((p_Rx_dataPacket[0] == 0x66) && (p_Rx_dataPacket[1] == 0x06))
            {
                print_allowed = false;
            }

            if (print_allowed)
            {
                // clang-format off
                ci_printv("\r\n\n[SPI RX] (LSB) 0x%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                          "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (MSB) \r\n",
                          p_Rx_dataPacket[0], p_Rx_dataPacket[1], p_Rx_dataPacket[2], p_Rx_dataPacket[3],
                          p_Rx_dataPacket[4], p_Rx_dataPacket[5], p_Rx_dataPacket[6], p_Rx_dataPacket[7],
                          p_Rx_dataPacket[8], p_Rx_dataPacket[9], p_Rx_dataPacket[10], p_Rx_dataPacket[11],
                          p_Rx_dataPacket[12], p_Rx_dataPacket[13], p_Rx_dataPacket[14], p_Rx_dataPacket[15],
                          p_Rx_dataPacket[16], p_Rx_dataPacket[17], p_Rx_dataPacket[18], p_Rx_dataPacket[19],
                          p_Rx_dataPacket[20]);
                // clang-format on
            }
#endif

#if 0  // Recover 디버깅 용
            if (p_Rx_dataPacket[0] == en__remoteControl_recover_ALL_SlotData_ManufactureData)
            {
            	Sys_GPIO_Set_High(DIO19);
            }
#endif

            // 명령에 따른 함수 실행
            if (p_Rx_dataPacket[0] == en__bleSetting_ReadConnected_ISD_info)
            {
                fetch_readDataForBleSetting(p_Rx_dataPacket);
            }
            // 리모콘
            // 헤더 0x40에서 0x5F까지 (실제 유효한 마지막 헤더는 0x59 en__remoteControl_systemOperatingMode 까지)
            else if ((en__remoteControl_check_isd_passKey <= p_Rx_dataPacket[0])  //
                     && (p_Rx_dataPacket[0] < en__mapping_connect))
            {
                fetch_remoteControlPacket(p_Rx_dataPacket);
            }
            // 매핑
            // 헤더 0x60에서 0x91까지 (실제 유효한 마지막 헤더는 0x71 en__mapping_recover_ALL_SlotData_ManufactureData 까지)
            else if ((en__mapping_connect <= p_Rx_dataPacket[0])  //
                     && (p_Rx_dataPacket[0] <= en__mapping_read_Connected_ISD_id))
            {
                fetch_mappingControlPacket(p_Rx_dataPacket);
            }
            // Sound1에서 추가된 QCC와 EZ 사이의 특수 명령어
            else if ((EN__SND_BT_CMD_SYSTEM_INFO_BATTERY <= p_Rx_dataPacket[0])      // 0x33 배터리 정보 부터
                     && (p_Rx_dataPacket[0] <= EN__SND_BT_CMD_SYSTEM_INFO_LED_IND))  // 0x35 LED 표시 까지
            {
                // NOTE: 별도의 함수를 만들어야 하지만,
                // 우선은 부팅 시 초기에 수행되는 en__bleSetting_ReadConnected_ISD_info 명령과 동일한
                // fetch_readDataForBleSetting() 함수를 사용하도록 한다.
                // 패치된 명령 처리도 마찬가지로, setting_nrf_ble_adv_info() 함수에서 한다.
                fetch_readDataForBleSetting(p_Rx_dataPacket);
            }
            // DFU (OTA) 관련 명령어
            else if (p_Rx_dataPacket[0] == PKT_HEADER_BOOT)  // 0xC2, BOOT 명령 (상태 읽기, 슬롯 선택하기)
            {
                ci_ble_fetch_packet_boot(p_Rx_dataPacket);
            }
            else if ((p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA_START)    // 0xC0, DFU (OTA) 시작 명령
                     || (p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA_END))  // 0xC1, DFU (OTA) 종료 명령
            {
                ci_ble_fetch_packet_ota_start_end(p_Rx_dataPacket);
            }
            else if (p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA)  // 0xC3, DFU (OTA) 데이터 명령
            {
                ci_ble_fetch_packet_ota(p_Rx_dataPacket);
            }
            else
            {
            }

            set_spi_commu_state_IDLE();

#endif
        }
        break;

        case SPI_CMMM_ERROR:
        {
            // SPI 인터페이스 에러
            NVIC_DisableIRQ(SPI1_COM_IRQn);

            // Disable the SPI before configure the SPI port
            Sys_SPI_TransferConfig(SPI1, DRIVER_SPI_CTRL_DISABLE);

            Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);  // DMA0 끄기
            Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);  // DMA1 끄기

            NVIC_ClearPendingIRQ(SPI1_COM_IRQn);
            NVIC_ClearPendingIRQ(DMA0_IRQn);
            NVIC_ClearPendingIRQ(DMA1_IRQn);

            // Clear flags
            SPI1->STATUS = DRIVER_SPI_STATUS;

            clear_SPI_Tx_Buffer();

            ci_SPI_enable_DMA();

            set_spi_commu_state_IDLE();

            // Enable SPI
            Sys_SPI_TransferConfig(SPI1, DRIVER_SPI_CTRL_ENABLE);

            NVIC_ClearPendingIRQ(SPI1_COM_IRQn);
            NVIC_EnableIRQ(SPI1_COM_IRQn);

            // SPI 플래그 신호는 High, Low 상태 상관 없이 QCC가 타임아웃으로
            // 처리하도록 현재 상태를 유지한다.
        }
        break;

        default:
            break;
    }

    //
    setting_nrf_ble_adv_info();

    //
    remoteControlState = remoteControl(isd_state.conneded_ISD);

    //
    mappingState = mappingControl(isd_state);

    // 매핑과 리모콘은 동시에 연결되지 못한다.

    // 매핑에서 제어되는 데이터 업데이트

    if (mappingState.isdControlCommand != en__isdStatus_NA)
    {
        ble_communication_state.isdControlCommand = mappingState.isdControlCommand;
    }
    else
    {
        ble_communication_state.isdControlCommand = en__isdStatus_NA;
    }

    // ble_communication_state 는 반환되는 구조체이다.
    // ble_communication_state.isdControlCommand는 위의 if-else 구조로 인해 무조건
    // en__isdStatus_NA 또는 어떠한 명령 값을 가질 수 밖에 없다.

    if (mappingState.BLE_Off)
    {
        ble_communication_state.BLE_Off_Command = true;
    }
    else
    {
        ble_communication_state.BLE_Off_Command = false;
    }

#if 1  // 현재는 리모콘에서 전달 받은 값으로 내부기 제어상태를 변경을 적용한 곳은 없다.
    // 리모콘에서 제어되는 데이터 업데이트
    if (remoteControlState.isdControlCommand != en__isdStatus_NA)
    {
        ble_communication_state.isdControlCommand = remoteControlState.isdControlCommand;
    }
#endif

    if (remoteControlState.BLE_Off)
    {
        ble_communication_state.BLE_Off_Command = true;
    }

    ble_communication_state.StimulationIndicatorTrigger = mappingState.StimulationIndicatorTrigger;
    ble_communication_state.mappingConnection           = mappingState.mappingConnection;

    // BLE 연결이 해제되면 패스키 확인을 클리어시킴
    // 매핑 연결이 해제된 경우는 추가로 매핑 연결 상태도 연결 해제로 초기화 시킴
    if (p_Rx_dataPacket[0] == en__BLE_Disconnected_Flag)
    {
        p_Rx_dataPacket[0] = 0;
#if 1
        clear_isd_passKeyMatchResult();
        clearRemoteColtrolCommand();

        if (mappingState.mappingConnection)
        {
            changeMappingCommandBleDisconneted();
            ble_communication_state.mappingConnection = false;
            ble_communication_state.isdControlCommand = en__isdStatus_PowerIC_Reset;

            // 매핑 앱 연결 상태에서 블루투스 연결이 끊어진 경우에는
            // 내부기 연결 과정을 RX PMIC 5V 리셋부터 다시 시작하도록, 반환되는 값인
            // ble_communication_state 구조체의 isdControlCommand의 값을 en__isdStatus_PowerIC_Reset 로 설정한다.
        }

        //ci_event_log_write(CI_EVENT_LOG_TYPE_DISCONNECTED);
        ci_event_log_update_bt_addr(NULL);
#endif
    }

    return ble_communication_state;
}
