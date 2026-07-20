
#include <hw.h>  // 디버깅용
#include <stdbool.h>

#include "remoteControl.h"
#include "tdc_remote_general_debug.h"
#include "tdc_remote_gain_control.h"
#include "remoteControl_read_SP_para.h"
#include "driver_SPI.h"
#include "definitionsForAlgorithm.h"
#include "cfx_cm3_sharedMemory.h"
#include "board.h"  // 디버깅용

#include "stimulationParaCal.h"
#include "batteryNPowerControl.h"
#include "systemControl.h"
#include "error.h"
#include <ci_ble_control_ota.h>

#include "isd_interface_mapping_readWrtieMapData.h"
#include "isd_interface_init_ISD.h"

// 송신할  데이터가 준비 되면  set_spi_commu_state_IDLE()를 호출한다.

ST__REMOTECONTROL_PACKET remoteDataPacket;

bool isd_PassKeyMatchResult = false;

void set_isd_passKeyMatchResult(void)
{
    isd_PassKeyMatchResult = true;
}

void clear_isd_passKeyMatchResult(void)
{
    isd_PassKeyMatchResult = false;
}

bool is_isd_passKeyMatch(void)
{
    return isd_PassKeyMatchResult;
}

void clearRemoteColtrolCommand(void)
{
    remoteDataPacket.command = en__remoteControl_IDLE;
}

void changeRemoteCommandWaitingForBleOff(void)
{
    remoteDataPacket.command = en__remoteControl_waiting_for_BleOff;
}

EN__REMOTE_CONTROL_COMMAND getRemoteCommand(void)
{
    return remoteDataPacket.command;
}

static int writingStartSlot_index = 0;

void fetch_remoteControlPacket(const int *Rx_dataPacket)
{
    static int prev_subCommandData_Num_index = 0;
    static int stimulPara_index              = 0;

    int bufferForSPI_tx[BLE_DataPacketSize];
    int tempCommand;
    int tempValue;

    int  subCommandData_Num_index;
    int *p_RepositoryFor_ISD_info;
    int *p_RepositoryFor_stimulPara;
    int  intFromByte;

    int buffer_tx_index = 0;
    int index           = 0;
    int i;

    bool dataRangeError = false;

    tempCommand = Rx_dataPacket[index++];  // 패킷 헤더 추출

    /// 이전 명령에 대한 응답이 완료되기 전에 새 명령이 수신된 경우
    if (remoteDataPacket.command != en__remoteControl_IDLE)
    {
        sendErrorToApp(tempCommand, en__EN__BLE_PROTOCOL_ERROR, en__PreviouCommnadIsNotCompleted, __LINE__);
        tempCommand = en__remoteControl_IDLE;  // 현재 받은 명령을 수행하지 않는다.
    }

    switch (tempCommand)
    {
        case en__remoteControl_read_SlotData_ISD_N_USER:  // 0x4A
        {
            tempValue = Rx_dataPacket[index++];

            // 슬롯 번호 1~4인 경우만 허용
            if ((1 <= tempValue) && (tempValue <= MaxNumUser))
            {
                remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = tempValue;
                remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = 0;
                remoteDataPacket.command                                   = en__remoteControl_read_SlotData_ISD_N_USER;
            }
            else
            {
                // 에러 전송 (데이터 범위를 벗어남)
                sendErrorToApp(en__remoteControl_read_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                clearRemoteColtrolCommand();
            }
        }
        break;

        case en__remoteControl_write_SlotData_ISD_N_USER:  // 0x4C
        {
            p_RepositoryFor_ISD_info = getPointerRepositoryForReadWriteMapData_isd_info();

            subCommandData_Num_index = Rx_dataPacket[index++];  // 데이터 인덱스

            if (subCommandData_Num_index == 1)  // 데이터 인덱스가 1인 경우, 백업 인덱스 초기화
            {
                prev_subCommandData_Num_index = 0;
            }

            // 데이터가 순차적으로 들어와야함, 순차적으로 들어 오지 않으면 에러 전송
            if (subCommandData_Num_index != (prev_subCommandData_Num_index + 1))
            {
                // 에러 전송
                sendErrorToApp(en__remoteControl_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남

                prev_subCommandData_Num_index = 0;
                stimulPara_index              = 0;
                clearRemoteColtrolCommand();
            }
            else
            {
                prev_subCommandData_Num_index = subCommandData_Num_index;  // 백업 인덱스 업데이트

                switch (subCommandData_Num_index)
                {
                    case 1:  // 데이터 인덱스 1
                    {
                        tempValue = Rx_dataPacket[index++];  // 슬롯 번호

                        if ((1 <= tempValue) && (tempValue <= MaxNumUser))
                        {
                            remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                            // 총 17 바이트: 내부기 ID (4) + 수술 위치 (1) + 사용자 이름 (12)
                            for (i = 0; i < 17; i++)
                            {
                                switch (i)
                                {
                                    case 0:  // 내부기 ID: 년 (8bit)
                                        p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];
                                        break;

                                    case 1:  // 내부기 ID: 월 (4bit) + 모델번호 (4bit)
                                        p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];
                                        break;

                                    case 2:  // 내부기 ID: 시리얼 (상위 8bit)
                                        tempValue = Rx_dataPacket[index++];
                                        tempValue = tempValue << 8;
                                        break;

                                    case 3:  // 내부기 ID: 시리얼 (하위 8bit)
                                        p_RepositoryFor_ISD_info[i - 1] = tempValue | Rx_dataPacket[index++];
                                        // 시리얼 번호를 16bit으로 조합하면서 실제 참조해야할 포인터의 인덱스 감소 [i-1]
                                        break;

                                    case 4:  // 수술 위치 (8bit)
                                        p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]

                                        // 수술 위치가 1:좌, 2:우, 3:공장초기화 가 아니면 에러
                                        if ((p_RepositoryFor_ISD_info[i - 1] < 1) || (3 < p_RepositoryFor_ISD_info[i - 1]))
                                        {
                                            dataRangeError = true;
                                        }
                                        break;

                                    default:
                                        p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                        break;
                                }
                            }
                        }
                        else
                        {
                            dataRangeError = true;
                        }
                    }
                    break;

                    case 2:
                    {
                        // 총 18 바이트: 사용자 이름 나머지 (13) + 내부기 패스키 (4) + 맵 번호 (1)
                        for (i = 17; i < 35; i++)
                        {
                            switch (index)
                            {
                                    // 내부기 패스키: 0~9, A~Z(대문자)
                                case 13:
                                case 14:
                                case 15:
                                case 16:
                                    tempValue = Rx_dataPacket[index];
                                    if (tempValue < '0' || '9' < tempValue)
                                    {
                                        if (tempValue < 'A' || 'Z' < tempValue)
                                        {
                                            dataRangeError = true;
                                        }
                                    }
                                    break;

                                    // 맵 번호: 1~4
                                case 17:
                                    tempValue = Rx_dataPacket[index];
                                    if (tempValue < 1 || 4 < tempValue)
                                    {
                                        dataRangeError = true;
                                    }
                                    break;
                            }

                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  // 시리얼 번호를 16bit으로 조합하면서 실제 참조해야할 포인터의 인덱스 감소 [i-1]
                        }
                    }
                    break;

                    case 3:
                    {
                        // 자극 볼륨 (최대 출력): 1~4
                        i         = 35;
                        tempValue = Rx_dataPacket[index++];  // i=35
                        if ((tempValue < 1) || (4 < df_maxStimulationVloumeLevel))
                        {
                            dataRangeError = true;
                        }
                        else
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }

                        // 마이크 감도 (볼륨): 1~10
                        i++;  // i=36
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue < 1) || (df_maxMicVloumeLevel < tempValue))
                        {
                            dataRangeError = true;
                        }
                        else
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }

                        // LED 설정: 1~2
                        i++;  // i=37
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue < 1) || (2 < tempValue))
                        {
                            dataRangeError = true;
                        }
                        else
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }

                        // 자극 알림 설정: 1~2
                        i++;  // i=38
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue < 1) || (2 < tempValue))
                        {
                            dataRangeError = true;
                        }
                        else
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }

                        // telecoil 설정: 1~2
                        i++;  // i=39
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue < 1) || (2 < tempValue))
                        {
                            dataRangeError = true;
                        }
                        else
                        {
#if 0
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
#else  // Telecoil은 현재 버전에서 비활성화 시킨다.
                            p_RepositoryFor_ISD_info[i - 1] = 2;
#endif
                        }

                        //  자극 볼륨, 마이크 감도, LED 설정, 자극 알림 설정, 텔레 코일 설정,

                        // BLE On/OFF 옵션
                        i++;  // i=40
                        // tempValue=Rx_dataPacket[index++]; 현재는 프로토콜에는 없음.
                        p_RepositoryFor_ISD_info[i - 1] = 1;  // BLE On/OFF 옵션

                        for (i = 41; i < 47; i++)
                        {
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //   맵 스템프
                        }

                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                    }
                    break;

                    default:
                    {
                    }
                    break;
                }

                if (!dataRangeError)
                {
                    if (subCommandData_Num_index != numPacket_writeMapData_ISDnSetting)
                    {
                        // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = en__remoteControl_write_SlotData_ISD_N_USER;

                        // payload num 전송
                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;  // payload num 전송

                        // 송신 데이터 SPI TX버퍼에 복사
                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
                    }
                    else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
                    {
                        prev_subCommandData_Num_index = 0;
                        remoteDataPacket.command      = en__remoteControl_write_SlotData_ISD_N_USER;
                        // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
                    }
                }
                else
                {
                    // 에러 전송
                    // 데이터 범위를 벗어남
                    sendErrorToApp(en__remoteControl_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    clearRemoteColtrolCommand();
                }
            }
        }
        break;  // en__remoteControl_write_SlotData_ISD_N_USER 끝

        case en__remoteControl_read_Mapdata_STIMUL_PARA:
        {

            tempValue = Rx_dataPacket[index++];
            if ((tempValue >= 0) && (tempValue <= MaxNumUser))
            {

                remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = tempValue;
                tempValue                                                  = Rx_dataPacket[index++];
                if ((tempValue >= 1) && (tempValue <= MaxNumMap))
                {
                    remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index = tempValue;
                    remoteDataPacket.command                                  = en__remoteControl_read_Mapdata_STIMUL_PARA;
                }
                else
                {
                    // 에러 전송
                    // 데이터 범위를 벗어남
                    sendErrorToApp(en__remoteControl_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    clearRemoteColtrolCommand();
                }
            }
            else
            {
                // 에러 전송
                // 데이터 범위를 벗어남
                sendErrorToApp(en__remoteControl_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                clearRemoteColtrolCommand();
            }
        }
        break;

        case en__remoteControl_write_Mapdata_STIMUL_PARA:
        {

            p_RepositoryFor_stimulPara = getPointerRepositoryForReadWriteMapData_stimulPara();

            subCommandData_Num_index = Rx_dataPacket[index++];
            if (subCommandData_Num_index == 1)
            {
                prev_subCommandData_Num_index = 0;
            }

            if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                // 에러 전송
                sendErrorToApp(en__remoteControl_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남

                prev_subCommandData_Num_index = 0;

                clearRemoteColtrolCommand();
            }
            else
            {
                prev_subCommandData_Num_index = subCommandData_Num_index;
                switch (subCommandData_Num_index)
                {
                    case 1:  //
                    {

                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue >= 0) && (tempValue <= MaxNumUser))
                        {

                            remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                            tempValue = Rx_dataPacket[index++];

                            if ((tempValue >= 1) && (tempValue <= MaxNumMap))
                            {
                                remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index = tempValue;  // 프로그램 번호

                                stimulPara_index = 0;
                                for (i = 0; i < 12; i++)
                                {
                                    p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  // 매핑 일자, 자극 펄스 파라미터 들..
                                }

                                intFromByte                                    = Rx_dataPacket[index++] << 8;           // 자극 알람 크기, 상위 바이트
                                intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                                p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;
                            }
                            else
                            {
                                // 데이터 범위 에러 발생
                                dataRangeError = true;
                            }
                        }
                        else
                        {
                            // 데이터 범위 에러 발생
                            dataRangeError = true;
                        }
                    }
                    break;

                    case 2:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  // 사용가능 전극 번호 18개
                        }
                    }
                    break;

                    case 3:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  // 사용가능 전극 번호 12개
                        }
                    }
                    break;

                    case 4:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 18개
                        }
                    }
                    break;

                    case 5:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 12개
                        }
                    }
                    break;

                    case 6:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 18개
                        }
                    }
                    break;

                    case 7:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 12개
                        }
                    }
                    break;

                    case 8:
                    case 9:
                    case 10:
                    case 11:
                    {
                        // T_uA 자극 크기
                        for (i = 0; i < 8; i++)
                        {
                            intFromByte                                    = Rx_dataPacket[index++] << 8;           // 상위 바이트
                            intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                            p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;
                        }
                    }
                    break;

                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    {
                        // C_uA 자극 크기
                        for (i = 0; i < 8; i++)
                        {
                            intFromByte                                    = Rx_dataPacket[index++] << 8;           // 상위 바이트
                            intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                            p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;
                        }
                    }
                    break;

                    default:
                    {
                    }
                    break;
                }

                if (!dataRangeError)
                {

                    if (subCommandData_Num_index != numPacket_writeMapData_stimulPara)
                    {
                        // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = en__remoteControl_write_Mapdata_STIMUL_PARA;

                        // payload num 전송

                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;  // payload num 전송

                        // 송신 데이터 SPI TX버퍼에 복사
                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
                    }
                    else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
                    {

                        // xmin
                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = df_minAudioForLogarithm;
                        }

                        // xmax
                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = df_maxAudioForLogarithm;
                        }

                        prev_subCommandData_Num_index = 0;
                        stimulPara_index              = 0;

                        remoteDataPacket.command = en__remoteControl_write_Mapdata_STIMUL_PARA;

                        // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
                    }
                }
                else
                {
                    sendErrorToApp(en__remoteControl_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남

                    clearRemoteColtrolCommand();
                }
            }
        }
        break;
        case en__remoteControl_erase_SlotData:
        {

            remoteDataPacket.command                                   = tempCommand;
            remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
            remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = 0;  // 삭제할 맵 번호가 0이면 모든 슬롯의 맵 데이터를 지운다.(at CFX)
        }
        break;

        case en__remoteControl_erase_mapData_STIMUL_PARA:
        {

            remoteDataPacket.command                                   = tempCommand;
            remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
            remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
        }
        break;

        case en__remoteControl_erase_mppingData_exceptSlot_1:
        {

            writingStartSlot_index                                     = 2;
            remoteDataPacket.command                                   = tempCommand;
            remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
            remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
        }
        break;

        case en__remoteControl_recover_ALL_SlotData_ManufactureData:
        {
            writingStartSlot_index   = 1;
            remoteDataPacket.command = tempCommand;
        }
        break;

        default:  // 위의 Flash memory  명령을 제외한 기본 명령
        {
            remoteDataPacket.command = tempCommand;
            for (i = 0; i < todoc_PayloadSize; i++)
            {
                remoteDataPacket.data[i] = Rx_dataPacket[index++];
            }
        }
        break;
    }
}

/*
en__remoteControl_userName=0x45,
en__remoteControl_deviceStatus,
en__remoteControl_changeMapNum,
en__remoteControl_changeStimulVolume,
en__remoteControl_changeMicVolume,
en__remoteControl_TeleCoilOnOff,
en__remoteControl_StimulaionIndicatorOnOff,
en__remoteControl_LED_OnOff,
en__remoteControl_ReadSystemError
*/

#define df_lengthOf_mapDate       6
#define df_DisConnectionCheckTime 180000

extern char *readFirmwareInfo();

ST__REMOTECONTROL_STATE remoteControl(bool isdConnection)  // 연결 상태에 변화에 따라 패스키 리셋
{
    static int                        noCommandTime_counter = df_DisConnectionCheckTime;
    static EN__REMOTE_CONTROL_COMMAND prev_remotegCommand   = en__remoteControl_IDLE;
    static int                        disconnectionCounter  = 0;
    ST__REMOTECONTROL_STATE           RemoteControlState    = {en__isdStatus_NA, false};

    int bufferForSPI_tx[BLE_DataPacketSize];
    int connectedISD_num, volume;
    int tx_index = 0;
    int i, k;

    int *p_connected_isd_usableMapIndex;
    int  currenMapIndex, nextMapIndex;
    int  value;
    int  iterNum;
    int *p_conectedISD_remoconPasskey;
    int *p_mapStemp;

    int  *p_currentUserName;
    int  *p_connected_ids_mapDate;
    int   connected_isd_usableMapNum;
    int   connected_isd_mapDate[4][6];
    int   numbering[4];
    int   find_index;
    int   numOfSame;
    int   maxValue, tempValue;
    char *p_firmwareInfo;

    bool remocon_passkey_Match;
    bool remoteCommandStartFlag = false;
    bool result;

#if 0  //
    //매핑 명령이 수신되 시접에 내부기 연결 확인용 backtel 전송 명령이 실행 중일 경우에는 백텔 수신이 완료되고 명령을 실행 할 수 있도록 한다.
    if(remoteDataPacket.fetched_command!=en__remoteControl_IDLE)
    {
        if(readCurrentPcmOutputMode()==BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation)
        {
            remoteDataPacket.command=remoteDataPacket.fetched_command;
            remoteDataPacket.fetched_command=en__remoteControl_IDLE;

        }
    }
#endif

    if (!isdConnection)
    {
        disconnectionCounter++;
        if (disconnectionCounter >= 1500)
        {
            clear_isd_passKeyMatchResult();
            disconnectionCounter = 1500;
        }
    }
    else
    {
        disconnectionCounter = 0;
    }

    if (prev_remotegCommand != remoteDataPacket.command)
    {
        remoteCommandStartFlag = true;
    }
    else
    {
        remoteCommandStartFlag = false;
    }

    prev_remotegCommand = remoteDataPacket.command;

    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);

    // remoteDataPacket.command=en__BLE_COMM_COMMAND_readInfoOfMap;

    if (en__remoteControl_check_isd_passKey == remoteDataPacket.command)
    {
        connectedISD_num             = read_connected_ISD_Num();
        p_conectedISD_remoconPasskey = read_recomcon_passkey_connected_ISD(connectedISD_num);
        remocon_passkey_Match        = true;

#if 0  // IMPORTANT: 패스키 인증을 더 이상 사용하지 않는다.
        for (i = 0; i < 4; i++)
        {
            if (p_conectedISD_remoconPasskey[i] != remoteDataPacket.data[i])
            {
                remocon_passkey_Match = false;
                break;
            }
        }
#endif

        ci_printw("[PASSKEY] ALWAYS PATH OPENED. \r\n");

        // 송신 데이터 준비
        // command loop-back
        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;

        // pay-load 준비
        if (remocon_passkey_Match)
        {
#if 1  // 로그 기능
            CI_TIMER_TIME_T        time;
            CI_EVENT_LOG_BT_ADDR_T bt_addr;

            // 참조 시간 정보
            time.year  = remoteDataPacket.data[4];
            time.month = remoteDataPacket.data[5];
            time.day   = remoteDataPacket.data[6];
            time.hour  = remoteDataPacket.data[7];
            time.min   = remoteDataPacket.data[8];
            time.sec   = remoteDataPacket.data[9];

            ci_timer_update_reference_time(&time, 0);

            // 블루투스 주소 정보
            bt_addr.bt_addr[0] = remoteDataPacket.data[10];
            bt_addr.bt_addr[1] = remoteDataPacket.data[11];
            bt_addr.bt_addr[2] = remoteDataPacket.data[12];
            bt_addr.bt_addr[3] = remoteDataPacket.data[13];
            bt_addr.bt_addr[4] = remoteDataPacket.data[14];
            bt_addr.bt_addr[5] = remoteDataPacket.data[15];

            ci_event_log_update_bt_addr(&bt_addr);

            // 로그 쓰기 : 블루투스 연결
            ci_event_log_write(CI_EVENT_LOG_TYPE_CONNECTED);
#endif
            bufferForSPI_tx[tx_index++] = 1;

            // 패스키 매치 업데이트
            set_isd_passKeyMatchResult();
        }
        else
        {
            bufferForSPI_tx[tx_index++] = 2;
        }

        // 송신 데이터 SPI TX버퍼에 복사
        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);

        //  명령 종료
        clearRemoteColtrolCommand();
    }
    else
    {
        if (is_isd_passKeyMatch())
        {
            if (remoteDataPacket.command == en__remoteControl_IDLE)
            {
                noCommandTime_counter++;
            }
            else
            {
                noCommandTime_counter = 0;
            }

            switch (remoteDataPacket.command)
            {
                case en__remoteControl_readInfoOfExtenalDevice:
                {
                    // NRF에서 바로 응답하기 때문에 Ezairo로 명령이 수신되지 않는다.
                }
                break;

                case en__remoteControl_readInfoOfMap:
                {
#if 0  // 맵에 들어있는 맵핑일자들에서 최신 맵일자를 전송하는 경우
                    for (i = 0; i < MaxNumMap; i++)
                    {
                        p_connected_ids_mapDate = readConnected_ISD_MapDate(i + 1);
                        for (k = 0; k < 6; k++)
                        {
                            connected_isd_mapDate[i][k] = *p_connected_ids_mapDate++;
                        }
                    }

                    for (i = 0; i < MaxNumMap; i++)
                    {
                        numbering[i] = 1;
                    }

                    for (i = 0; i < df_lengthOf_mapDate; i++)
                    {
                        startFlag = true;
                        for (k = 0; k < MaxNumMap; k++)
                        {
                            if (numbering[k] == 1)
                            {
                                if (startFlag)  // 새로운 열의 첫 검색 데이터를  최대값으로 설정하고 시작
                                {
                                    maxValue  = connected_isd_mapDate[k][i];
                                    startFlag = false;
                                }
                                else
                                {
                                    if (connected_isd_mapDate[k][i] > maxValue)  // 새로운 배교값이 이전 최대값 보다 클 경우
                                    {
                                        maxValue = connected_isd_mapDate[k][i];

                                        // 이전에 검색된 최대 값 인덱스를 지움
                                        for (m = 0; m < k; m++)
                                        {
                                            numbering[m] = 0;
                                        }
                                        // 현재 검색된 인덱스를 최대값으로 선택
                                        numbering[k] = 1;
                                    }
                                    else if (connected_isd_mapDate[k][i] == maxValue)
                                    {
                                        // 현재 검색된 인덱스를 최대값으로 추가
                                        numbering[k] = 1;
                                    }
                                    else  // 새로운 배교값이 이전 최대값 보다 작을 경우
                                    {
                                        numbering[k] = 0;
                                    }
                                }
                            }
                        }

                        numOfSame = 0;
                        for (m = 0; m < MaxNumMap; m++)
                        {
                            if (numbering[m] == 1)
                            {
                                numOfSame++;
                            }
                        }

                        if (numOfSame == 1)
                        {
                            break;
                        }
                    }

                    // 최대값 인덱스 확인
                    for (m = 0; m < MaxNumMap; m++)
                    {
                        if (numbering[m] == 1)
                        {
                            find_index = m;
                        }
                    }

                    // 송신 데이터 준비
                    // command loop-back
                    Tx_dataBuff[tx_index++] = remoteDataPacket.command;

                    // pay-load 준비
                    // 최신 맵 날짜
                    for (k = 0; k < df_lengthOf_mapDate; k++)
                    {
                        Tx_dataBuff[tx_index++] = connected_isd_mapDate[find_index][k];
                    }

                    // 담겨져 있는 맵 개수
                    Tx_dataBuff[tx_index++] = readConnected_ISD_usableMapNum();

                    // 송신 데이터 SPI TX버퍼에 복사

                    writeDataToSpiTxBuff(Tx_dataBuff, tx_index);

                    //  명령 종료
                    clearRemoteColtrolCommand();
#else
                    // 현재 연결된 맵 스템프 전송

                    // 송신 데이터 준비
                    // command loop-back
                    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;

                    // pay-load 준비
                    // 최신 맵 날짜
                    p_mapStemp = readConnected_ISD_MapStamp();

                    for (k = 0; k < df_lengthOf_mapDate; k++)
                    {
                        bufferForSPI_tx[tx_index++] = p_mapStemp[k];
                    }

                    // 담겨져 있는 맵 개수
                    bufferForSPI_tx[tx_index++] = readConnected_ISD_usableMapNum();

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);

                    //  명령 종료
                    clearRemoteColtrolCommand();
#endif
                }
                break;
#if 0
                case en__remoteControl_readUserName:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    Tx_dataBuff[tx_index++] = remoteDataPacket.command;

                    // pay-load 준비
                    connectedISD_num  = read_connected_ISD_Num();
                    p_currentUserName = readConnected_ISD_userName(connectedISD_num);

                    for (i = 0; i < todoc_PayloadSize; i++)
                    {
                        Tx_dataBuff[tx_index++] = p_currentUserName[i];
                    }

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(Tx_dataBuff, tx_index);

                    //  명령 종료
                    remoteDataPacket.command = en__remoteControl_IDLE;
                }
                break;
#endif
                case en__remoteControl_readStatusOfExtenalDevice:  // 0x43
                {
                    value = readBatteryPercentage();

                    if (value > 100)
                    {
                        value = 100;
                    }
                    else if (value < 0)
                    {
                        value = 0;
                    }

                    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;     // command loop-back
                    bufferForSPI_tx[tx_index++] = value;                        // 배터리 잔량
                    bufferForSPI_tx[tx_index++] = readProgramMapNum();          // 맵 번호
                    bufferForSPI_tx[tx_index++] = readStimulVolume();           // 최대 출력
                    bufferForSPI_tx[tx_index++] = readAudioVolume();            // 볼륨
                    bufferForSPI_tx[tx_index++] = readLED_indicatorOnOff();     // LED 알림
                    bufferForSPI_tx[tx_index++] = readTeleCoil_OnOff();         // 텔레코일
                    bufferForSPI_tx[tx_index++] = readStimulIndicator_OnOff();  // 자극 알림

                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    clearRemoteColtrolCommand();                      //  명령 종료
                }
                break;

                case en__remoteControl_changeMapNum:
                {
                    currenMapIndex                 = readProgramMapNum();
                    nextMapIndex                   = currenMapIndex;
                    p_connected_isd_usableMapIndex = readConnected_ISD_usableMapIndex();
                    value                          = remoteDataPacket.data[0];
                    iterNum                        = 0;

                    if (value == 1)  // 맵 번호 증기
                    {
                        do  // 사용가능한 맵에서 회전 시킨다.
                        {
                            nextMapIndex++;
                            if (nextMapIndex > MaxNumMap)
                            {
                                nextMapIndex = 1;
                            }

                            iterNum++;
                            if (iterNum > MaxNumMap)
                            {
                                break;
                            }
                        } while (p_connected_isd_usableMapIndex[nextMapIndex - 1] == 0);

                        if (iterNum <= MaxNumMap)
                        {
                            changeProgramMapNum(nextMapIndex);

                            // 송신 데이터 준비
                            bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                            bufferForSPI_tx[tx_index++] = nextMapIndex;              // pay-load 준비

                            writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                            clearRemoteColtrolCommand();                      //  명령 종료
                        }
                        else
                        {
                            sendErrorToApp(remoteDataPacket.command, en__CFX_ERROR, en__unusableMapData, __LINE__);
                            clearRemoteColtrolCommand();
                        }
                    }
                    else if (value == 2)  // 맵 번호 감소
                    {
                        do
                        {
                            nextMapIndex--;
                            if (nextMapIndex < 1)
                            {
                                nextMapIndex = MaxNumMap;
                            }

                            iterNum++;
                            if (iterNum > MaxNumMap)
                            {
                                break;
                            }
                        } while (p_connected_isd_usableMapIndex[nextMapIndex - 1] == 0);

                        if (iterNum <= MaxNumMap)
                        {
                            changeProgramMapNum(nextMapIndex);

                            // 송신 데이터 준비
                            bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                            bufferForSPI_tx[tx_index++] = nextMapIndex;              // pay-load 준비

                            writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                            clearRemoteColtrolCommand();                      //  명령 종료
                        }
                        else
                        {
                            sendErrorToApp(remoteDataPacket.command, en__CFX_ERROR, en__unusableMapData, __LINE__);
                            clearRemoteColtrolCommand();
                        }
                    }
                    else  // 에러: 입력 데이터 범위 1~2가 아닌 경우
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_adjustStimulationVolume:  // 0x45
                {
                    if ((remoteDataPacket.data[0] == en__PAYLOAD_INCREASE) || (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE))
                    {
                        volume = readStimulVolume();

                        if (remoteDataPacket.data[0] == en__PAYLOAD_INCREASE)
                        {
                            if (volume < df_maxStimulationVloumeLevel)
                            {
                                volume++;
                            }
                            changeStimulVolume(volume);
                        }
                        else if (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE)
                        {
                            if (volume > 1)
                            {
                                volume--;
                            }
                            changeStimulVolume(volume);
                        }

                        // 송신 데이터 준비
                        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                        bufferForSPI_tx[tx_index++] = readStimulVolume();        // pay-load 준비

                        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                        clearRemoteColtrolCommand();                      //  명령 종료
                    }
                    else  // 데이터 범위 에러
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_adjustMicVolume:  // 0x46
                {
                    if ((remoteDataPacket.data[0] == en__PAYLOAD_INCREASE) || (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE))
                    {
                        volume = readAudioVolume();

                        if (remoteDataPacket.data[0] == en__PAYLOAD_INCREASE)
                        {
                            if (volume < df_maxMicVloumeLevel)
                            {
                                volume++;
                            }
                            changeAudioVolume(volume);
                        }
                        else if (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE)
                        {
                            if (volume > 1)
                            {
                                volume--;
                            }
                            changeAudioVolume(volume);
                        }

                        // 송신 데이터 준비
                        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                        bufferForSPI_tx[tx_index++] = readAudioVolume();         // pay-load 준비

                        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                        clearRemoteColtrolCommand();                      //  명령 종료
                    }
                    else  // 데이터 범위 에러
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_OnOffTelecoil:  // 0x47
                {
                    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
                    {
                        changeTeleCoil_OnOff(remoteDataPacket.data[0]);

                        // 송신 데이터 준비
                        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                        bufferForSPI_tx[tx_index++] = readTeleCoil_OnOff();      // pay-load 준비

                        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                        clearRemoteColtrolCommand();                      //  명령 종료
                    }
                    else  // 데이터 범위 에러
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_OnOffStimulationIndicator:  // 0x48
                {
                    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
                    {
                        changeStimulIndicator_OnOff(remoteDataPacket.data[0]);

                        // 송신 데이터 준비
                        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;     // command loop-back
                        bufferForSPI_tx[tx_index++] = readStimulIndicator_OnOff();  // pay-load 준비

                        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                        clearRemoteColtrolCommand();                      // 명령 종료
                    }
                    else  // 데이터 범위 에러
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_OnOffLED:  // 0x49
                {
                    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
                    {
                        changeLED_indicatorOnOff(remoteDataPacket.data[0]);

                        // 송신 데이터 준비
                        bufferForSPI_tx[tx_index++] = remoteDataPacket.command;  // command loop-back
                        bufferForSPI_tx[tx_index++] = readLED_indicatorOnOff();  // pay-load 준비

                        writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                        clearRemoteColtrolCommand();                      //  명령 종료
                    }
                    else  // 데이터 범위 에러
                    {
                        sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                        clearRemoteColtrolCommand();
                    }
                }
                break;

                case en__remoteControl_readSoundSignal:
                {
                    read_signal_processingPara(remoteCommandStartFlag, en__remoteControl_readSoundSignal);
                }
                break;

                case en__remoteControl_read_Ezairo_FirmwareInfo:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;
                    p_firmwareInfo              = readFirmwareInfo();

                    // pay-load 준비
                    for (i = 0; i < 14; i++)
                    {
                        bufferForSPI_tx[tx_index++] = p_firmwareInfo[i];
                    }

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);

                    //  명령 종료
                    clearRemoteColtrolCommand();
                }
                break;

                case en__remoteControl_read_SlotData_ISD_N_USER:
                {
                    read_isdInfo_N_userSetting_fromFlash(remoteCommandStartFlag, en__remoteControl_read_SlotData_ISD_N_USER, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__remoteControl_write_SlotData_ISD_N_USER:
                {
                    write_isdInfo_N_userSetting_atFlash(remoteCommandStartFlag, en__remoteControl_write_SlotData_ISD_N_USER, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__remoteControl_read_Mapdata_STIMUL_PARA:
                {
                    read_stimulPara_fromFlash(remoteCommandStartFlag, en__remoteControl_read_Mapdata_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__remoteControl_write_Mapdata_STIMUL_PARA:
                {
                    write_stimulPara_atFlash(remoteCommandStartFlag, en__remoteControl_write_Mapdata_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__remoteControl_erase_SlotData:
                {
                    reset_NVM_Selected_ISD_allData(remoteCommandStartFlag, en__remoteControl_erase_SlotData, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, flash_Command_Erase);
                }
                break;

                case en__remoteControl_erase_mapData_STIMUL_PARA:
                {
                    reset_NVM_MapData(remoteCommandStartFlag, en__remoteControl_erase_mapData_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index, flash_Command_Erase);
                }
                break;

                case en__remoteControl_erase_mppingData_exceptSlot_1:
                {
                    reset_NVM_2to4_ISD_allData(remoteCommandStartFlag, en__remoteControl_erase_mppingData_exceptSlot_1, flash_Command_Erase);
                }
                break;

                case en__remoteControl_recover_ALL_SlotData_ManufactureData:
                {
                    ci_printi("[PACKET] RECEIVED, RECOVER ALL SLOT DATA MANUFACTURE DATA \r\n");

                    result = reset_NVM_All_ISD_allData(remoteCommandStartFlag, en__remoteControl_recover_ALL_SlotData_ManufactureData, flash_Command_Recover);

                    if (result)
                    {
                        ci_printi("[PACKET] RESULT : TRUE, AFTER RESET NVM ALL ISD ALL DATA \r\n");

                        while (1)
                        {
                            if (isSpiTxBuffEmpty())
                            {
                                // changeSystemModeFlag(en__systemReset);  // 제조용 리모콘에서 페어링키 쓰기 명령을 보낼때 nrf에서 페어링키를 쓰고 리셋을
                                // 한다. 따라서 여기서 리셋하는 경우가 발생하면 전송에러가 발생된다. 명령 종료
                                if (en__remoteControl_recover_ALL_SlotData_ManufactureData > 0x60)
                                {
                                    clear_mappingCommand();
                                }
                                else
                                {
                                    clearRemoteColtrolCommand();
                                }
                                break;
                            }
                        }

                        // Sys_GPIO_Set_Low(DIO19);
                    }
                }
                break;

#if 0  // 프로토콜 3.8부터 삭제되었으나 리모콘 에러시 확인 필요
                case en__remoteControl_read_Connected_ISD_id:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;

                    // pay-load 준비
                    value = read_Connected_ISD_id();

                    bufferForSPI_tx[tx_index++] = value >> 24;
                    bufferForSPI_tx[tx_index++] = 0xff & (value >> 16);
                    bufferForSPI_tx[tx_index++] = 0xff & (value >> 8);
                    bufferForSPI_tx[tx_index++] = 0xff & (value);

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);

                    //  명령 종료
                    clearRemoteColtrolCommand();
                }
                break;
#endif

#if 1
                /**
                 * 26.06.23 범용 디버깅 프로토콜 기능에 대한 코드
                 * by 김은수
                 * 26.07.01 tdc_remote_general_debug.c 로 분리 */
                case EN__SND_BT_CMD_GENERAL_DEBUG:
                {
                    tx_index = tdc_remote_general_debug_handle(&remoteDataPacket, bufferForSPI_tx, tx_index);

                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    clearRemoteColtrolCommand();                      // 명령 종료
                }
                break;
#endif

#if 1
                /**
                 * 26.07.20 Gain Conversion Table 인덱스 설정 프로토콜 (0x8C)
                 * by 김은수 */
                case EN__SND_BT_CMD_GAIN_CONTROL:
                {
                    tx_index = tdc_remote_gain_control_handle(&remoteDataPacket, bufferForSPI_tx, tx_index);

                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    clearRemoteColtrolCommand();                      // 명령 종료
                }
                break;
#endif

#if 1
                /**
                 * 26.01.19 CFX의 묵음 처리 기능 활성화/비활성화를 위해 추가한 기능.
                 * 무선 프로토콜 문서 v4.0.2의 시스템 동작 모드 패킷 (0x59)에 대한 처리 구문.
                 * by 김은수 */
                case en__remoteControl_specificSystemOperationSetting:
                {
                    switch (remoteDataPacket.data[0])  // 옵션
                    {
                        case 1:  // 읽기 (현재 상태 값으로 응답)
                        {
                            ci_printd("[MUTE] RECEVIED : READ PACKET \r\n");

                            // 송신 데이터 준비
                            bufferForSPI_tx[tx_index++] = remoteDataPacket.command;                                                 //     command : loop-back
                            bufferForSPI_tx[tx_index++] = 1;                                                                        //      option : read
                            bufferForSPI_tx[tx_index++] = 1;                                                                        // sub option1 : normal mode
                            bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level;  // sub option2 : enable state
                            bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset;            // sub option3 : mute t level offset

                            ci_printd("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

                            writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                            clearRemoteColtrolCommand();                      // 명령 종료
                        }
                        break;

                        case 2:  // 쓰기
                        {
                            switch (remoteDataPacket.data[1])  // 세부 옵션 1
                            {
                                case 1:  // 일반 모드
                                {
                                    switch (remoteDataPacket.data[2])  // 세부 옵션 2
                                    {
                                        case 1:  // 묵음 처리 활성화
                                        {
                                            ci_printd("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE + T LEVEL OFFSET %d \r\n", remoteDataPacket.data[3]);

                                            // 설정 가능 범위 초과 시 에러
                                            if ((remoteDataPacket.data[3] < CI_STIM_MUTE_T_LEVEL_OFFSET_MIN) || (CI_STIM_MUTE_T_LEVEL_OFFSET_MAX < remoteDataPacket.data[3]))
                                            {
                                                ci_printe("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE, BUT INVALID T OFFSET LEVEL \r\n");

                                                sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                                                clearRemoteColtrolCommand();
                                            }
                                            else  // 유효한 설정 값인 경우
                                            {
                                                // 묵음 처리 파일 및 공유 메모리 값 업데이트
                                                if (ci_stim_mute_update(CI_STIM_MUTE_UNDER_T_LEVEL_ENABLE, (uint32_t) remoteDataPacket.data[3]) != CI_STIM_MUTE_RET_TRUE)
                                                {
                                                    ci_printe("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE, BUT FAILED TO UPDATE FILE \r\n");

                                                    // 실패 시 에러 전송: 데이터 처리 에러 + 사용할 수 없는 맵데이터
                                                    sendErrorToApp(remoteDataPacket.command, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                                                    clearRemoteColtrolCommand();
                                                }
                                                else  // 묵음 처리 파일 및 공유 메모리 값 업데이트 성공
                                                {
                                                    // 송신 데이터 준비
                                                    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;                                                 //     command : loop-back
                                                    bufferForSPI_tx[tx_index++] = 2;                                                                        //      option : write
                                                    bufferForSPI_tx[tx_index++] = 1;                                                                        // sub option1 : normal mode
                                                    bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level;  // sub option2 : enable state
                                                    bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset;            // sub option3 : mute t level offset

                                                    ci_printd("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

                                                    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                                                    clearRemoteColtrolCommand();                      // 명령 종료
                                                }
                                            }
                                        }
                                        break;

                                        case 2:  // 묵음 처리 비활성화
                                        {
                                            // 묵음 처리 비활성화 옵션에서는 세부 옵션 3은 N/A 처리 함
                                            // 결과적으로 현재 옵션 레벨을 그대로 사용하면 될 것으로 보임

                                            ci_printd("[MUTE] RECEVIED : WRITE NORMAL + DISABLE MUTE \r\n");

                                            // 묵음 처리 파일 및 공유 메모리 값 업데이트
                                            if (ci_stim_mute_update(CI_STIM_MUTE_UNDER_T_LEVEL_DISABLE, cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset) != CI_STIM_MUTE_RET_TRUE)
                                            {
                                                ci_printe("[MUTE] RECEVIED : WRITE NORMAL + DISABLE MUTE, BUT FAILED TO UPDATE FILE \r\n");

                                                // 실패 시 에러 전송: 데이터 처리 에러 + 사용할 수 없는 맵데이터
                                                sendErrorToApp(remoteDataPacket.command, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                                                clearRemoteColtrolCommand();
                                            }
                                            else  // 묵음 처리 파일 및 공유 메모리 값 업데이트 성공
                                            {
                                                // 송신 데이터 준비
                                                bufferForSPI_tx[tx_index++] = remoteDataPacket.command;                                                 //     command : loop-back
                                                bufferForSPI_tx[tx_index++] = 2;                                                                        //      option : write
                                                bufferForSPI_tx[tx_index++] = 1;                                                                        // sub option1 : normal mode
                                                bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level;  // sub option2 : enable state
                                                bufferForSPI_tx[tx_index++] = (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset;            // sub option3 : mute t level offset

                                                ci_printd("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

                                                writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                                                clearRemoteColtrolCommand();                      // 명령 종료
                                            }
                                        }
                                        break;

                                        default:
                                        {
                                            ci_printe("[MUTE] RECEVIED : WRITE NORMAL OPTION, BUT UNDEFINED SUB OPTION 2 PACKET \r\n");

                                            sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                                            clearRemoteColtrolCommand();
                                        }
                                        break;
                                    }  // 끝, switch for 세부 옵션 2
                                }
                                break;

                                case 2:  // 진단 모드 (현재는 N/A)
                                {
                                    ci_printe("[MUTE] RECEVIED : WRITE DIAGNOSTICS PACKET, BUT N/A CURRENTLY \r\n");

                                    sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                                    clearRemoteColtrolCommand();
                                }
                                break;

                                default:
                                {
                                    ci_printe("[MUTE] RECEVIED : UNDEFINED SUB OPTION 1 PACKET \r\n");

                                    sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                                    clearRemoteColtrolCommand();
                                }
                                break;
                            }  // 끝, switch for 세부 옵션 1
                        }
                        break;

                        default:  // 기타 정의되지 않은 옵션
                        {
                            ci_printe("[MUTE] RECEVIED : UNDEFINED OPTOIN PACKET \r\n");

                            sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                            clearRemoteColtrolCommand();
                        }
                        break;
                    }  // 끝, switch for 옵션

                }  // 끝, case en__remoteControl_specificSystemOperationSetting:
                break;
#endif

                case en__remoteControl_IDLE :
                {
                }
                break;

            }  // End, "switch (remoteDataPacket.command)"
        }      // End, "if (is_isd_passKeyMatch())"
        else
        {
            if(remoteDataPacket.command != en__remoteControl_IDLE)
            {
                // 송신 데이터 SPI TX버퍼에 복사
                sendErrorToApp(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR ,en__NO_SECURITY, __LINE__ );
                //  명령 종료
                clearRemoteColtrolCommand();
            }
            else
            {
            }
        }
    }

    return RemoteControlState;
}
