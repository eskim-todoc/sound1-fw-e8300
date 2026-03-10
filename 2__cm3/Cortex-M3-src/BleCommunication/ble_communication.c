

#include <stdbool.h>
#include "driver_SPI.h"
#include "isd_interface.h"
#include "remoteControl.h"
#include "mappingControl.h"
#include "ble_communication.h"

typedef struct
{
    ReadCommandForBleSetting command;
    int                      data[todoc_PayloadSize];

} BleSettingPacket;

BleSettingPacket bleSettingPacket;

void fetch_readDataForBleSetting(const int *Rx_dataPacket)
{
    bleSettingPacket.command = Rx_dataPacket[0];
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
        // 수술위치
        connectedISD_num        = read_connected_ISD_Num();
        Tx_dataBuff[tx_index++] = (int) readConnected_ISD_Location(connectedISD_num);

        // 사용자 이름
        p_currentUserName = readConnected_ISD_userName(connectedISD_num);
        for (i = 0; i < 10; i++)
            Tx_dataBuff[tx_index++] = p_currentUserName[i];

        // 송신 데이터 SPI TX버퍼에 복사

        writeDataToSpiTxBuff(Tx_dataBuff, tx_index);

        //  명령 종료
        bleSettingPacket.command = en__bleSetting_IDLE;
    }
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
                ci_printv("\r\n\n[SPI RX] (LSB) 0x%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                          "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (MSB) \r\n",
                          p_Rx_dataPacket[0],
                          p_Rx_dataPacket[1],
                          p_Rx_dataPacket[2],
                          p_Rx_dataPacket[3],
                          p_Rx_dataPacket[4],
                          p_Rx_dataPacket[5],
                          p_Rx_dataPacket[6],
                          p_Rx_dataPacket[7],
                          p_Rx_dataPacket[8],
                          p_Rx_dataPacket[9],
                          p_Rx_dataPacket[10],
                          p_Rx_dataPacket[11],
                          p_Rx_dataPacket[12],
                          p_Rx_dataPacket[13],
                          p_Rx_dataPacket[14],
                          p_Rx_dataPacket[15],
                          p_Rx_dataPacket[16],
                          p_Rx_dataPacket[17],
                          p_Rx_dataPacket[18],
                          p_Rx_dataPacket[19],
                          p_Rx_dataPacket[20]);
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
            else if ((en__remoteControl_check_isd_passKey <= p_Rx_dataPacket[0]) && (p_Rx_dataPacket[0] < en__mapping_connect))
            {
                fetch_remoteControlPacket(p_Rx_dataPacket);
            }
            // 매핑
            // 헤더 0x60에서 0x91까지 (실제 유효한 마지막 헤더는 0x71 en__mapping_recover_ALL_SlotData_ManufactureData 까지)
            else if ((en__mapping_connect <= p_Rx_dataPacket[0]) && (p_Rx_dataPacket[0] <= en__mapping_read_Connected_ISD_id))
            {
                fetch_mappingControlPacket(p_Rx_dataPacket);
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
