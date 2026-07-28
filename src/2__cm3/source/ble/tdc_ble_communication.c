

#include <stdbool.h>
#include <tdc_hal_spi.h>
#include <tdc_isd.h>
#include <tdc_ble_remote.h>
#include <tdc_ble_mapping.h>
#include <tdc_ble_communication.h>

#include <tdc_pwr_battery.h>

#include <tdc_dfu_ble_boot.h>
#include <tdc_dfu_ble_ota.h>

#include <tdc_hal_timer.h>
#include <tdc_printf.h>

#include <tdc_shm.h>  // cfx_cm3_sharedMemoryAll (게인 테이블 연동)
#include <tdc_ble_reply.h>

typedef struct
{
    ReadCommandForBleSetting command;
    int                      data[todoc_PayloadSize];

} BleSettingPacket;

BleSettingPacket bleSettingPacket;

void fetch_readDataForBleSetting(const uint8_t *Rx_dataPacket)
{
    bleSettingPacket.command = Rx_dataPacket[0];

    // NOTE: Sound1에서 추가된 QCC와의 특수 명령에 대해서,
    // 헤더 외에 데이터가 있는 특수 패킷의 경우 데이터를 복사해야 한다.
    // 현재는 0x34, Power info만 데이터가 존재하는 상태이지만,
    // 추후에 얼마나 명령어가 늘어날지 예측할 수 없다. (2026.03.12)

    if ((EN__SND_BT_CMD_SYSTEM_INFO_POWER <= Rx_dataPacket[0])              // 0x34 POWER INFO 부터
        && (Rx_dataPacket[0] <= EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE))  // 0x36 클래식 상태 표시 까지
    {
        for (int i = 0; i < todoc_PayloadSize; i++)
        {
            bleSettingPacket.data[i] = Rx_dataPacket[1 + i];
        }
    }
}

void setting_nrf_ble_adv_info(void)
{

    uint8_t Tx_dataBuff[BLE_DataPacketSize];

    int  connectedISD_num;
    int *p_currentUserName;

    int tx_index = 0;
    int i;

    if (bleSettingPacket.command == en__bleSetting_ReadConnected_ISD_info)
    {
        ST__ISD_STATUS isd_status;

        isd_status = tdc_isd_get_state();

        // ISD가 연결된 상태라면 내부기 정보 전달
        if (isd_status.conneded_ISD)
        {
            // 수술위치
            connectedISD_num        = tdc_shm_read_connected_isd_num();
            tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, (uint8_t) tdc_shm_read_connected_isd_location(connectedISD_num));

            // 사용자 이름
            p_currentUserName = tdc_shm_read_connected_isd_user_name(connectedISD_num);
            for (i = 0; i < 10; i++)
            {
                tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, p_currentUserName[i]);
            }
        }
        // ISD가 연결되지 않은 상태라면 0으로 채운 더미 데이터 전달
        // TX 크기가 0이면 SPI TX 버퍼에서 알아서 21바이트를 0으로 채워서 전달
        // 전송 크기가 0이므로 전송 바이트 수를 알려주는 마지막 바이트
        // 즉, [20] 인덱스도 0으로 채워져서 보내질 것이다.
        else
        {
            TDC_PRINTF_W("[BT] ISD NOT CONNECTED, BUT RESPONSE 0x30 COMMAND \r\n");
            tx_index = 0;
        }

        tdc_hal_spi_write_tx_buffer(Tx_dataBuff, tx_index);     // 송신 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  //  명령 종료
    }
    // QCC와 새로 추가한 패킷 (0x33. Battery 정보)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_BATTERY)
    {
        int batt_percent;
        int charger_state;

        // 배터리 레벨을 QCC에게 수신한 이후로만 0xFF가 아닌 값을 전송한다.
        // 사실상 QCC가 배터리 레벨을 측정하기로 한 뒤로 쓸모가 없는 명령이 되었다.
        if (tdc_pwr_battery_get_state() != TDC_PWR_BATTERY_STATE_RESET)
        {
            batt_percent = tdc_pwr_battery_get_percent();
            TDC_PRINTF_D("[BT] READ BATT LEVEL, %d PERCENT \r\n", batt_percent);
        }
        else
        {
            batt_percent = 0xFF;
            TDC_PRINTF_D("[BT] READ BATT LEVEL NOT YET READY \r\n");
        }

        charger_state = tdc_pwr_charger_get_state().chargerConnectorPluggedIn;

        tx_index = tdc_ble_reply_header(Tx_dataBuff, tx_index, EN__SND_BT_CMD_SYSTEM_INFO_BATTERY);
        tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, batt_percent);
        tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, charger_state);  // 0: RESET, 1: CONNECTED, 2: DISCONNECTED

        tdc_hal_spi_write_tx_buffer(Tx_dataBuff, tx_index);     // 송싱 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // QCC와 새로 초가한 패킷 (0x34, Power info)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_POWER)
    {
        int battery_level;      // 패킷 인덱스 1 → 헤더 제외 시, 데이터 인덱스 0
        int charger_connected;  // 패킷 인덱스 2 → 헤더 제외 시, 데이터 인덱스 1
        int cradle_lid_state;   // 패킷 인덱스 3 → 헤더 제외 시, 데이터 인덱스 2 (1=열림, 2=닫힘, else=열림처리)

        battery_level     = bleSettingPacket.data[0];  // 배터리 레벨
        charger_connected = bleSettingPacket.data[1];  // 충전기 연결 상태
        cradle_lid_state  = bleSettingPacket.data[2];  // 크래들 뚜껑 상태

        // 수신한 배터리 정보로 업데이트 한다.
        tdc_pwr_battery_set_percent(battery_level);

        // 배터리 충전 상태인지 방전 즉, 일반 동작 상태인지는
        // 충전기 연결 상태에 따라서 배터리 상태 업데이트를 진행해야 한다.

        switch (charger_connected)
        {
            case 0:  // Disconnected
                tdc_pwr_charger_set_state(TDC_PWR_CHARGER_STATE_DISCONNECTED);
                tdc_pwr_battery_set_state(TDC_PWR_BATTERY_STATE_DISCHARGING);
                break;

            case 1:  // Connected
                tdc_pwr_charger_set_state(TDC_PWR_CHARGER_STATE_CONNECTED);
                tdc_pwr_battery_set_state(TDC_PWR_BATTERY_STATE_CHARGING);
                break;

            default:
                TDC_PRINTF_E("[BT] CMD 0x%02X, INVALID CHARGER CONNECTED: %d \r\n", EN__SND_BT_CMD_SYSTEM_INFO_POWER, charger_connected);
                tdc_pwr_charger_set_state(TDC_PWR_CHARGER_STATE_RESET);
                tdc_pwr_battery_set_state(TDC_PWR_BATTERY_STATE_RESET);
                break;
        }  // 끝, switch

        tdc_pwr_cradle_set_cover_state(cradle_lid_state);
        TDC_PRINTF_V("[BT] CMD 0x%02X, CHARGER STATE: %d, BATT LEVEL %d PERCENT, LID STATE %d\r\n", EN__SND_BT_CMD_SYSTEM_INFO_POWER, charger_connected, battery_level, cradle_lid_state);

        tx_index = tdc_ble_reply_header(Tx_dataBuff, tx_index, EN__SND_BT_CMD_SYSTEM_INFO_POWER);
        tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, 1);  // 수신 확인 응답

        // TDC_PRINTF_W("[BT] BEFORE-WRITE-TX 0x34 t3=%d ms\r\n", tdc_hal_timer_get_t3_tick());
        // TDC_PRINTF_W("[BT] CALL-WRITE-TX TxEmpty=%d\r\n", (int) tdc_hal_spi_is_tx_buffer_empty());

        tdc_hal_spi_write_tx_buffer(Tx_dataBuff, tx_index);     // 송싱 데이터 SPI TX버퍼에 복사
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

        if (led_ind == TDC_LED_IND_STATE_OTA_EZAIRO)
        {
            tdc_dfu_set_conn_state(TDC_DFU_CONN_ST_CONN);
        }
        else
        {
            tdc_dfu_set_conn_state(TDC_DFU_CONN_ST_DISCONN);
        }

        TDC_PRINTF_V("[BT] CMD 0x%02X, LED IND: %d \r\n", EN__SND_BT_CMD_SYSTEM_INFO_LED_IND, led_ind);

        /* 응답 패킷 */
        tx_index = tdc_ble_reply_header(Tx_dataBuff, tx_index, EN__SND_BT_CMD_SYSTEM_INFO_LED_IND);
        tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, 1);                     // 수신 확인 응답
        tdc_hal_spi_write_tx_buffer(Tx_dataBuff, tx_index);     // 송신 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // 끝, else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_LED_IND)
    // 시작, QCC와 새로 초가한 패킷 (0x36, 클래식 상태 표시)
    else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE)
    {
        int classic_state;  // 패킷 인덱스 1 →헤더 제외 시, 데이터 인덱스 0
        int classic_type;   // 패킷 인덱스 2 →헤더 제외 시, 데이터 인덱스 1

        classic_state = bleSettingPacket.data[0];  // 클래식 상태
        classic_type  = bleSettingPacket.data[1];  // 클래식 종류

        /* 명령 처리 */
        TDC_PRINTF_W("[BT] CMD 0x%02X, CLASSIC STATE: %s, %s \r\n",  //
                  classic_state == 0   ? "DISCONN"
                  : classic_state == 1 ? "CONN"
                                       : "INVALID",
                  classic_type == 0   ? "UNKNOWN"
                  : classic_type == 1 ? "CRADLE"
                  : classic_type == 2 ? "OTHER"
                                      : "INVALID");

        /* I2S 로 들어오는 오디오가 크래들 마이크인지 CFX 에 알려 준다.
         * CFX 는 이 값이 1 이면 I2S 경로에 Gain_B 를 적용하고,
         * 0(스트리밍)이면 스마트폰이 볼륨을 제어하므로 게인을 적용하지 않는다. */
        cfx_cm3_sharedMemoryAll.is_i2s_source_cradle = ((classic_state == 1) && (classic_type == 1)) ? 1 : 0;

        /* 응답 패킷 */
        tx_index = tdc_ble_reply_header(Tx_dataBuff, tx_index, EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE);
        tx_index = tdc_ble_reply_u8(Tx_dataBuff, tx_index, 1);                     // 수신 확인 응답
        tdc_hal_spi_write_tx_buffer(Tx_dataBuff, tx_index);     // 송신 데이터 SPI TX버퍼에 복사
        bleSettingPacket.command = en__bleSetting_IDLE;  // 명령 종료
    }
    // 끝, else if (bleSettingPacket.command == EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE)
}

ST__BLE_COMMUNICATION_STATE tdc_ble_communication_step(ST__ISD_STATUS isd_state)
{
    int             i;
    static uint8_t *p_Rx_dataPacket;

    tdc_hal_spi_comm_state_t         communicationState;
    ST__MAPPING_STATE           mappingState;
    ST__BLE_COMMUNICATION_STATE ble_communication_state = {en__isdStatus_NA, false, false, false};
    ST__REMOTECONTROL_STATE     remoteControlState      = {en__isdStatus_NA, false};

    communicationState = tdc_hal_spi_get_comm_state();

    // SPI 통신으로 nRF로부터 패킷을 수신하면 communicationState가 SPI_CMMM_FETCH로 업데이트 됨
    // SPI 통신에 이슈가 발생한 경우 SPI_CMMM_ERROR로 업데이트 됨
    switch (communicationState)
    {
        case TDC_HAL_SPI_COMM_IDLE:
        {
            i++;
        }
        break;

        case TDC_HAL_SPI_COMM_FETCH:
        {
            /* nRF 에서 받은 패킷을 그대로 되돌려 보내고 마지막 바이트에 수신 횟수를 붙이던
             * 초기 SPI 루프백 검증 코드는 2차 리팩토링에서 제거했다(#if 0 사장).
             * 현재는 아래처럼 수신 패킷을 실제 커맨드로 파싱해 처리한다.
             * 루프백 카운터로 쓰던 static Rx_counter 도 함께 제거했다. */
            p_Rx_dataPacket = tdc_hal_spi_get_rx_packet_addr();

#if 1  // nRF SPI 디버깅

            bool print_allowed = true;

            // 라이브모드의 실시간 전류 값을 제외하고 출력 (데이터 양이 너무 많음)
            if ((p_Rx_dataPacket[0] == 0x66) && (p_Rx_dataPacket[1] == 0x06))
            {
                print_allowed = false;
            }

            if (print_allowed)
            {
                /* 인덱스 20 은 출력하지 않는다 (2026-07-27).
                 * Rx_DataPacket 은 BLE_DataPacketSize(20) 원소라 유효 범위가 [0..19] 다.
                 * 예전에는 [20] 까지 찍었는데 이는 배열 범위 밖 읽기였고, 수신 데이터가
                 * 아니라 인접 정적 변수 값이 그대로 출력되던 것이다(전 로그에서 01 고정).
                 * SPI 로 들어오는 21번째 바이트는 HAL 의 복사 루프가 20개까지만 옮기므로
                 * CM3 에서는 애초에 보관하지 않는다. TX 방향의 21번째 바이트만 전송 길이로 유효하다. */
                // clang-format off
                TDC_PRINTF_V("\r\n\n[SPI RX] (LSB) 0x%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                          "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (MSB) \r\n",
                          p_Rx_dataPacket[0], p_Rx_dataPacket[1], p_Rx_dataPacket[2], p_Rx_dataPacket[3],
                          p_Rx_dataPacket[4], p_Rx_dataPacket[5], p_Rx_dataPacket[6], p_Rx_dataPacket[7],
                          p_Rx_dataPacket[8], p_Rx_dataPacket[9], p_Rx_dataPacket[10], p_Rx_dataPacket[11],
                          p_Rx_dataPacket[12], p_Rx_dataPacket[13], p_Rx_dataPacket[14], p_Rx_dataPacket[15],
                          p_Rx_dataPacket[16], p_Rx_dataPacket[17], p_Rx_dataPacket[18], p_Rx_dataPacket[19]);
                // clang-format on
            }
#endif

            /* Recover 명령 수신 시 DIO19 를 High 로 올려 로직분석기로 보던 디버그 코드는
             * 2차 리팩토링에서 제거했다(#if 0 사장). */

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
                tdc_ble_remote_fetch_packet(p_Rx_dataPacket);
            }
            // 매핑
            // 헤더 0x60에서 0x91까지 (실제 유효한 마지막 헤더는 0x71 en__mapping_recover_ALL_SlotData_ManufactureData 까지)
            else if (((en__mapping_connect <= p_Rx_dataPacket[0])                   //
                      && (p_Rx_dataPacket[0] <= en__mapping_waiting_for_BleOff))    //
                     || (p_Rx_dataPacket[0] == en__mapping_testStimulation)         //
                     || (p_Rx_dataPacket[0] == en__mapping_read_Connected_ISD_id))  //
            {
                tdc_ble_mapping_fetch_packet(p_Rx_dataPacket);
            }
            // Sound1에서 추가된 QCC와 EZ 사이의 특수 명령어
            else if ((EN__SND_BT_CMD_SYSTEM_INFO_BATTERY <= p_Rx_dataPacket[0])            // 0x33 배터리 정보 부터
                     && (p_Rx_dataPacket[0] <= EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE))  // 0x36 클래식 상태 표시 까지
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
                tdc_dfu_ble_fetch_boot(p_Rx_dataPacket);
            }
            else if ((p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA_START)    // 0xC0, DFU (OTA) 시작 명령
                     || (p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA_END))  // 0xC1, DFU (OTA) 종료 명령
            {
                tdc_dfu_ble_fetch_ota_start_end(p_Rx_dataPacket);
            }
            else if (p_Rx_dataPacket[0] == CI_BLE_OTA_COMMAND_OTA)  // 0xC3, DFU (OTA) 데이터 명령
            {
                tdc_dfu_ble_fetch_ota(p_Rx_dataPacket);
            }
            else if (p_Rx_dataPacket[0] == EN__SND_BT_CMD_GAIN_CONTROL)  // 0x8C 게인 제어 프로토콜
            {
                tdc_ble_remote_fetch_packet(p_Rx_dataPacket);
            }
            else if (p_Rx_dataPacket[0] == EN__SND_BT_CMD_GENERAL_DEBUG)  // 0x8F 범용 디버그 프로토콜
            {
                tdc_ble_remote_fetch_packet(p_Rx_dataPacket);
            }
            else
            {
            }

            tdc_hal_spi_set_comm_state_idle();

        }
        break;

        case SPI_CMMM_ERROR:
        {
            TDC_PRINTF_W("\r\n");
            TDC_PRINTF_W("################################################################\r\n");
            TDC_PRINTF_W("###  [SPI ERROR HANDLED]    t3 = %d ms\r\n", tdc_hal_timer_get_t3_tick());
            TDC_PRINTF_W("################################################################\r\n");
            TDC_PRINTF_W("\r\n");

            // SPI 인터페이스 에러
            NVIC_DisableIRQ(SPI1_COM_IRQn);

            // Disable the SPI before configure the SPI port
            Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_DISABLE);

            Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);  // DMA0 끄기
            Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);  // DMA1 끄기

            NVIC_ClearPendingIRQ(SPI1_COM_IRQn);
            NVIC_ClearPendingIRQ(DMA0_IRQn);
            NVIC_ClearPendingIRQ(DMA1_IRQn);

            // Clear flags
            SPI1->STATUS = TDC_HAL_SPI_STATUS;

            tdc_hal_spi_clear_tx_buffer();

            tdc_hal_spi_enable_dma();

            tdc_hal_spi_set_comm_state_idle();

            // Enable SPI
            Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_ENABLE);

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
    remoteControlState = tdc_ble_remote_step(isd_state.conneded_ISD);

    //
    mappingState = tdc_ble_mapping_step(isd_state);

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
        tdc_ble_remote_clear_passkey_match();
        tdc_ble_remote_clear_command();

        if (mappingState.mappingConnection)
        {
            tdc_ble_mapping_change_command_ble_disconnected();
            ble_communication_state.mappingConnection = false;
            ble_communication_state.isdControlCommand = en__isdStatus_PowerIC_Reset;

            // 매핑 앱 연결 상태에서 블루투스 연결이 끊어진 경우에는
            // 내부기 연결 과정을 RX PMIC 5V 리셋부터 다시 시작하도록, 반환되는 값인
            // ble_communication_state 구조체의 isdControlCommand의 값을 en__isdStatus_PowerIC_Reset 로 설정한다.
        }

        //tdc_fs_event_log_write(TDC_FS_EVENT_LOG_TYPE_DISCONNECTED);
        tdc_fs_event_log_update_bt_addr(NULL);
#endif
    }

    return ble_communication_state;
}
