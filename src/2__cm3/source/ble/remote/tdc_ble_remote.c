
#include <hw.h>  // 디버깅용
#include <stdbool.h>

#include <tdc_ble_remote.h>
#include <tdc_ble_general_debug.h>
#include <tdc_ble_gain_control.h>
#include <tdc_ble_remote_sp_para.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_shm.h>

#include <tdc_stim_para_cal.h>
#include <tdc_pwr_battery.h>
#include <tdc_sys_control.h>
#include <tdc_sys_error.h>
#include <tdc_dfu_ble_ota.h>

#include <tdc_isd_map_data.h>
#include <tdc_isd_init.h>
#include <tdc_ble_reply.h>

// 송신할  데이터가 준비 되면  tdc_hal_spi_set_comm_state_idle()를 호출한다.

ST__REMOTECONTROL_PACKET remoteDataPacket;

bool isd_PassKeyMatchResult = false;

void tdc_ble_remote_set_passkey_match(void)
{
    isd_PassKeyMatchResult = true;
}

void tdc_ble_remote_clear_passkey_match(void)
{
    isd_PassKeyMatchResult = false;
}

bool tdc_ble_remote_is_passkey_match(void)
{
    return isd_PassKeyMatchResult;
}

void tdc_ble_remote_clear_command(void)
{
    remoteDataPacket.command = en__remoteControl_IDLE;
}

static int writingStartSlot_index         = 0;
static int prev_subCommandData_Num_index = 0;  /* fetch_packet 지역 static 이었다. 0x4A·0x4C·0x4D 가 공유한다 */
static int stimulPara_index              = 0;  /* 동. 0x4C·0x4D 가 공유한다 */

/* ---------------------------------------------------------------------
 * 파싱 계층 - 명령별 함수 (2026-08-05 이월_2 분해)
 *
 * tdc_ble_remote_fetch_packet() 의 switch case 본문을 그대로 옮긴 것이다.
 * 로직은 손대지 않았다.
 *
 * index 를 값으로 받는 것은 switch 직후 함수가 끝나 갱신값을 되돌릴
 * 필요가 없기 때문이다. 공유 static(prev_subCommandData_Num_index ·
 * stimulPara_index)은 파일 수준이라 여기서 그대로 보인다.
 *
 * 이름 규약: tdc_ble_cmd_0xNN_parse_<이름>. 리모콘은 같은 명령이 파싱과
 * 실행 두 층에 모두 있어 _parse_ / _step_ 으로 층을 구분한다.
 * --------------------------------------------------------------------- */

/* 0x4A 슬롯 데이터 읽기 - 슬롯 번호 1~4 검사 */
static void tdc_ble_cmd_0x4A_parse_read_slot(const uint8_t *Rx_dataPacket, int index)
{
    int tempValue;

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
        tdc_sys_error_send_to_app(en__remoteControl_read_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x4C 슬롯 데이터 쓰기 - 데이터 인덱스 1~3 순차 수신 */
static void tdc_ble_cmd_0x4C_parse_write_slot(const uint8_t *Rx_dataPacket, int index)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tempValue;
    int subCommandData_Num_index;
    int *p_RepositoryFor_ISD_info;
    int buffer_tx_index = 0;
    int i;
    bool dataRangeError = false;

    p_RepositoryFor_ISD_info = tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info();

    subCommandData_Num_index = Rx_dataPacket[index++];  // 데이터 인덱스

    if (subCommandData_Num_index == 1)  // 데이터 인덱스가 1인 경우, 백업 인덱스 초기화
    {
        prev_subCommandData_Num_index = 0;
    }

    // 데이터가 순차적으로 들어와야함, 순차적으로 들어 오지 않으면 에러 전송
    if (subCommandData_Num_index != (prev_subCommandData_Num_index + 1))
    {
        // 에러 전송
        tdc_sys_error_send_to_app(en__remoteControl_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남

        prev_subCommandData_Num_index = 0;
        stimulPara_index              = 0;
        tdc_ble_remote_clear_command();
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
                    /* 텔레코일 설정은 현재 버전에서 강제 비활성이다.
                     * 리모콘이 보낸 값(tempValue)을 쓰지 않고 항상 2(꺼짐)를 저장한다.
                     * 원래 코드는 `= tempValue;` 였고 #if 0 으로 죽어 있어 정리했다.
                     * 되살리려면 아래 대입을 tempValue 로 바꾸면 된다. */
                    p_RepositoryFor_ISD_info[i - 1] = 2;
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
                buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__remoteControl_write_SlotData_ISD_N_USER);

                // payload num 전송
                buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, subCommandData_Num_index);  // payload num 전송

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
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
            tdc_sys_error_send_to_app(en__remoteControl_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            tdc_ble_remote_clear_command();
        }
    }
}

/* 0x4B 맵 데이터 읽기 - 슬롯·맵 번호 검사 */
static void tdc_ble_cmd_0x4B_parse_read_mapdata(const uint8_t *Rx_dataPacket, int index)
{
    int tempValue;


    tempValue = Rx_dataPacket[index++];
    if ((1 <= tempValue) && (tempValue <= MaxNumUser))
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
            tdc_sys_error_send_to_app(en__remoteControl_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            tdc_ble_remote_clear_command();
        }
    }
    else
    {
        // 에러 전송
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__remoteControl_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x4D 맵 데이터 쓰기 - 데이터 인덱스 1~15 순차 수신 */
static void tdc_ble_cmd_0x4D_parse_write_mapdata(const uint8_t *Rx_dataPacket, int index)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tempValue;
    int subCommandData_Num_index;
    int *p_RepositoryFor_stimulPara;
    int intFromByte;
    int buffer_tx_index = 0;
    int i;
    bool dataRangeError = false;


    p_RepositoryFor_stimulPara = tdc_shm_get_pointer_repository_for_read_write_map_data_stimul_para();

    subCommandData_Num_index = Rx_dataPacket[index++];
    if (subCommandData_Num_index == 1)
    {
        prev_subCommandData_Num_index = 0;
    }

    if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
    {
        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
        // 에러 전송
        tdc_sys_error_send_to_app(en__remoteControl_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남

        prev_subCommandData_Num_index = 0;

        tdc_ble_remote_clear_command();
    }
    else
    {
        prev_subCommandData_Num_index = subCommandData_Num_index;
        switch (subCommandData_Num_index)
        {
            case 1:  //
            {

                tempValue = Rx_dataPacket[index++];
                if ((1 <= tempValue) && (tempValue <= MaxNumUser))
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
                buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__remoteControl_write_Mapdata_STIMUL_PARA);

                // payload num 전송

                buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, subCommandData_Num_index);  // payload num 전송

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
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
            tdc_sys_error_send_to_app(en__remoteControl_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남

            tdc_ble_remote_clear_command();
        }
    }
}

/* 0x4E 슬롯 데이터 삭제 */
static void tdc_ble_cmd_0x4E_parse_erase_slot(const uint8_t *Rx_dataPacket, int index, int command)
{

    remoteDataPacket.command                                   = command;
    remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
    remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = 0;  // 삭제할 맵 번호가 0이면 모든 슬롯의 맵 데이터를 지운다.(at CFX)
}

/* 0x4F 맵 데이터 삭제 */
static void tdc_ble_cmd_0x4F_parse_erase_mapdata(const uint8_t *Rx_dataPacket, int index, int command)
{

    remoteDataPacket.command                                   = command;
    remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
    remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
}

/* 0x50 슬롯 1 을 제외한 매핑 데이터 삭제 */
static void tdc_ble_cmd_0x50_parse_erase_except_slot1(const uint8_t *Rx_dataPacket, int index, int command)
{

    writingStartSlot_index                                     = 2;
    remoteDataPacket.command                                   = command;
    remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
    remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
}

/* 0x57 전체 슬롯 공장 데이터 복구 */
static void tdc_ble_cmd_0x57_parse_recover_all(int command)
{
    writingStartSlot_index   = 1;
    remoteDataPacket.command = command;
}

/* 플래시 계열이 아닌 일반 명령 - 페이로드 19바이트 적재 */
static void tdc_ble_remote_parse_general_payload(const uint8_t *Rx_dataPacket, int index, int command)
{
    int i;

    remoteDataPacket.command = command;
    for (i = 0; i < todoc_PayloadSize; i++)
    {
        remoteDataPacket.data[i] = Rx_dataPacket[index++];
    }
}

void tdc_ble_remote_fetch_packet(const uint8_t *Rx_dataPacket)
{
    int tempCommand;
    int index = 0;

    tempCommand = Rx_dataPacket[index++];  // 패킷 헤더 추출

    /// 이전 명령에 대한 응답이 완료되기 전에 새 명령이 수신된 경우
    if (remoteDataPacket.command != en__remoteControl_IDLE)
    {
        tdc_sys_error_send_to_app(tempCommand, en__EN__BLE_PROTOCOL_ERROR, en__PreviouCommnadIsNotCompleted, __LINE__);
        tempCommand = en__remoteControl_IDLE;  // 현재 받은 명령을 수행하지 않는다.
    }

    switch (tempCommand)
    {
        case en__remoteControl_read_SlotData_ISD_N_USER:  // 0x4A
        {
            tdc_ble_cmd_0x4A_parse_read_slot(Rx_dataPacket, index);
        }
        break;

        case en__remoteControl_write_SlotData_ISD_N_USER:  // 0x4C
        {
            tdc_ble_cmd_0x4C_parse_write_slot(Rx_dataPacket, index);
        }
        break;  // en__remoteControl_write_SlotData_ISD_N_USER 끝

        case en__remoteControl_read_Mapdata_STIMUL_PARA:
        {
            tdc_ble_cmd_0x4B_parse_read_mapdata(Rx_dataPacket, index);
        }
        break;

        case en__remoteControl_write_Mapdata_STIMUL_PARA:
        {
            tdc_ble_cmd_0x4D_parse_write_mapdata(Rx_dataPacket, index);
        }
        break;
        case en__remoteControl_erase_SlotData:
        {
            tdc_ble_cmd_0x4E_parse_erase_slot(Rx_dataPacket, index, tempCommand);
        }
        break;

        case en__remoteControl_erase_mapData_STIMUL_PARA:
        {
            tdc_ble_cmd_0x4F_parse_erase_mapdata(Rx_dataPacket, index, tempCommand);
        }
        break;

        case en__remoteControl_erase_mppingData_exceptSlot_1:
        {
            tdc_ble_cmd_0x50_parse_erase_except_slot1(Rx_dataPacket, index, tempCommand);
        }
        break;

        case en__remoteControl_recover_ALL_SlotData_ManufactureData:
        {
            tdc_ble_cmd_0x57_parse_recover_all(tempCommand);
        }
        break;

        default:  // 위의 Flash memory  명령을 제외한 기본 명령
        {
            tdc_ble_remote_parse_general_payload(Rx_dataPacket, index, tempCommand);
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

extern char *readFirmwareInfo();

/* ---------------------------------------------------------------------
 * 실행 계층 - 명령별 함수 (2026-08-05 이월_2 분해)
 *
 * tdc_ble_remote_step() 의 switch case 본문을 그대로 옮긴 것이다.
 * 로직은 손대지 않았다.
 *
 * 인자가 없는 것은 입력이 전역 remoteDataPacket 이고 출력이 SPI 송신과
 * 공유메모리이기 때문이다. 반환 상태(RemoteControlState)는 switch 안에서
 * 한 번도 대입되지 않으므로 넘길 필요가 없다.
 *
 * tx_index 를 각자 0 으로 두는 것은 switch case 가 배타적이라 성립한다
 * (단계_2 응답 조립 공통화에서 규명한 구조).
 *
 * 플래시 6건 · 0x8F · 게인제어 · 본문 없는 case 는 추출하지 않았다.
 * 각 4줄 이하 위임뿐이라 빼면 한 줄을 감싼 한 줄 함수가 된다.
 * --------------------------------------------------------------------- */

/* 0x42 맵 정보 읽기 - 사용 가능 맵 목록과 맵 스탬프 응답 */
static void tdc_ble_cmd_0x42_step_read_map_info(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;
    int *p_mapStemp;
    int k;

    /* 맵 4개의 매핑일자를 모두 비교해 가장 최신 것을 보내던 구버전 응답 로직은
     * 2차 리팩토링에서 제거했다(#if 0 사장, 약 100줄).
     * 현재 응답은 아래와 같이 "연결된 내부기의 맵 스탬프 + 보유 맵 개수" 다.
     * 리모콘과 주고받는 응답 형식이 서로 다르므로, 구형식이 필요하면 git 이력 참조. */
    // 현재 연결된 맵 스템프 전송

    // 송신 데이터 준비
    // command loop-back
    tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);

    // pay-load 준비
    // 최신 맵 날짜
    p_mapStemp = tdc_shm_read_connected_isd_map_stamp();

    for (k = 0; k < df_lengthOf_mapDate; k++)
    {
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, p_mapStemp[k]);
    }

    // 담겨져 있는 맵 개수
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_connected_isd_usable_map_num());

    // 송신 데이터 SPI TX버퍼에 복사
    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);

    //  명령 종료
    tdc_ble_remote_clear_command();
}

/* 0x43 외부기기 상태 읽기 - 배터리·볼륨·On/Off 상태 응답 */
static void tdc_ble_cmd_0x43_step_read_device_status(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;
    int value;

    value = tdc_pwr_battery_read_percentage();

    if (value > 100)
    {
        value = 100;
    }
    else if (value < 0)
    {
        value = 0;
    }

    tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);     // command loop-back
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, value);                        // 배터리 잔량
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_program_map_num());          // 맵 번호
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_stimul_volume());           // 최대 출력
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_audio_volume());            // 볼륨
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_led_indicator_on_off());     // LED 알림
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_tele_coil_on_off());         // 텔레코일
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_stimul_indicator_on_off());  // 자극 알림

    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
    tdc_ble_remote_clear_command();                      //  명령 종료
}

/* 0x44 맵 번호 변경 - 사용 가능한 다음 맵을 찾아 전환 */
static void tdc_ble_cmd_0x44_step_change_map_num(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;
    int *p_connected_isd_usableMapIndex;
    int currenMapIndex;
    int nextMapIndex;
    int value;
    int iterNum;

    currenMapIndex                 = tdc_shm_read_program_map_num();
    nextMapIndex                   = currenMapIndex;
    p_connected_isd_usableMapIndex = tdc_shm_read_connected_isd_usable_map_index();
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
            tdc_shm_change_program_map_num(nextMapIndex);

            // 송신 데이터 준비
            tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
            tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, nextMapIndex);              // pay-load 준비

            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
            tdc_ble_remote_clear_command();                      //  명령 종료
        }
        else
        {
            tdc_sys_error_send_to_app(remoteDataPacket.command, en__CFX_ERROR, en__unusableMapData, __LINE__);
            tdc_ble_remote_clear_command();
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
            tdc_shm_change_program_map_num(nextMapIndex);

            // 송신 데이터 준비
            tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
            tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, nextMapIndex);              // pay-load 준비

            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
            tdc_ble_remote_clear_command();                      //  명령 종료
        }
        else
        {
            tdc_sys_error_send_to_app(remoteDataPacket.command, en__CFX_ERROR, en__unusableMapData, __LINE__);
            tdc_ble_remote_clear_command();
        }
    }
    else  // 에러: 입력 데이터 범위 1~2가 아닌 경우
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x45 자극 볼륨 조절 - 1 단계 증감, 상하한에서 유지 */
static void tdc_ble_cmd_0x45_step_adjust_stim_volume(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int volume;
    int tx_index = 0;

    if ((remoteDataPacket.data[0] == en__PAYLOAD_INCREASE) || (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE))
    {
        volume = tdc_shm_read_stimul_volume();

        if (remoteDataPacket.data[0] == en__PAYLOAD_INCREASE)
        {
            if (volume < df_maxStimulationVloumeLevel)
            {
                volume++;
            }
            tdc_shm_change_stimul_volume(volume);
        }
        else if (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE)
        {
            if (volume > 1)
            {
                volume--;
            }
            tdc_shm_change_stimul_volume(volume);
        }

        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_stimul_volume());        // pay-load 준비

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      //  명령 종료
    }
    else  // 데이터 범위 에러
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x46 마이크 볼륨 조절 - 1 단계 증감 */
static void tdc_ble_cmd_0x46_step_adjust_mic_volume(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int volume;
    int tx_index = 0;

    if ((remoteDataPacket.data[0] == en__PAYLOAD_INCREASE) || (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE))
    {
        volume = tdc_shm_read_audio_volume();

        if (remoteDataPacket.data[0] == en__PAYLOAD_INCREASE)
        {
            if (volume < df_maxMicVloumeLevel)
            {
                volume++;
            }
            tdc_shm_change_audio_volume(volume);
        }
        else if (remoteDataPacket.data[0] == en__PAYLOAD_DECREASE)
        {
            if (volume > 1)
            {
                volume--;
            }
            tdc_shm_change_audio_volume(volume);
        }

        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_audio_volume());         // pay-load 준비

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      //  명령 종료
    }
    else  // 데이터 범위 에러
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x47 텔레코일 On/Off */
static void tdc_ble_cmd_0x47_step_onoff_telecoil(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;

    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
    {
        tdc_shm_change_tele_coil_on_off(remoteDataPacket.data[0]);

        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_tele_coil_on_off());      // pay-load 준비

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      //  명령 종료
    }
    else  // 데이터 범위 에러
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x48 자극 인디케이터 On/Off */
static void tdc_ble_cmd_0x48_step_onoff_stim_indicator(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;

    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
    {
        tdc_shm_change_stimul_indicator_on_off(remoteDataPacket.data[0]);

        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);     // command loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_stimul_indicator_on_off());  // pay-load 준비

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      // 명령 종료
    }
    else  // 데이터 범위 에러
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x49 LED On/Off */
static void tdc_ble_cmd_0x49_step_onoff_led(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;

    if ((remoteDataPacket.data[0] == en__PAYLOAD_ON) || (remoteDataPacket.data[0] == en__PAYLOAD_OFF))
    {
        tdc_shm_change_led_indicator_on_off(remoteDataPacket.data[0]);

        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);  // command loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, tdc_shm_read_led_indicator_on_off());  // pay-load 준비

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      //  명령 종료
    }
    else  // 데이터 범위 에러
    {
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        tdc_ble_remote_clear_command();
    }
}

/* 0x54 Ezairo 펌웨어 정보 읽기 */
static void tdc_ble_cmd_0x54_step_read_firmware_info(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int tx_index = 0;
    char *p_firmwareInfo;
    int i;

    // 송신 데이터 준비
    // command loop-back
    tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);
    p_firmwareInfo              = readFirmwareInfo();

    // pay-load 준비
    for (i = 0; i < 14; i++)
    {
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, p_firmwareInfo[i]);
    }

    // 송신 데이터 SPI TX버퍼에 복사
    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);

    //  명령 종료
    tdc_ble_remote_clear_command();
}

/* 0x59 특정 시스템 동작 설정 - 옵션(data[0])으로 읽기/쓰기 분기 */
/* 0x59 하위 분해 (2026-08-06). 옵션(data[0]) -> 세부옵션1(data[1]) ->
 * 세부옵션2(data[2]) 로 4중 중첩이던 것을 안쪽 세 갈래만 빼내 2중으로 줄였다.
 * 로직은 그대로 옮겼다. 이름의 sub/idx 는 프로토콜 계층을 그대로 따른다. */

/* 0x59 옵션2(쓰기) - 일반 모드 - 묵음 처리 비활성화 */
static void tdc_ble_cmd_0x59_sub02_idx02_mute_disable(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     tx_index = 0;

// 묵음 처리 비활성화 옵션에서는 세부 옵션 3은 N/A 처리 함
// 결과적으로 현재 옵션 레벨을 그대로 사용하면 될 것으로 보임

TDC_PRINTF_D("[MUTE] RECEVIED : WRITE NORMAL + DISABLE MUTE \r\n");

// 묵음 처리 파일 및 공유 메모리 값 업데이트
if (tdc_fs_stim_mute_update(TDC_FS_STIM_MUTE_UNDER_T_LEVEL_DISABLE, cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset) != TDC_FS_STIM_MUTE_RET_TRUE)
{
    TDC_PRINTF_E("[MUTE] RECEVIED : WRITE NORMAL + DISABLE MUTE, BUT FAILED TO UPDATE FILE \r\n");

    // 실패 시 에러 전송: 데이터 처리 에러 + 사용할 수 없는 맵데이터
    tdc_sys_error_send_to_app(remoteDataPacket.command, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
    tdc_ble_remote_clear_command();
}
else  // 묵음 처리 파일 및 공유 메모리 값 업데이트 성공
{
    // 송신 데이터 준비
    tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);                                                 //     command : loop-back
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 2);                                                                        //      option : write
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 1);                                                                        // sub option1 : normal mode
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level);  // sub option2 : enable state
    tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset);            // sub option3 : mute t level offset

    TDC_PRINTF_D("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
    tdc_ble_remote_clear_command();                      // 명령 종료
}
}

/* 0x59 옵션2(쓰기) - 일반 모드 - 묵음 처리 활성화 (T 레벨 오프셋 범위 검사 포함) */
static void tdc_ble_cmd_0x59_sub02_idx01_mute_enable(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     tx_index = 0;

TDC_PRINTF_D("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE + T LEVEL OFFSET %d \r\n", remoteDataPacket.data[3]);

// 설정 가능 범위 초과 시 에러
if ((remoteDataPacket.data[3] < TDC_FS_STIM_MUTE_T_LEVEL_OFFSET_MIN) || (TDC_FS_STIM_MUTE_T_LEVEL_OFFSET_MAX < remoteDataPacket.data[3]))
{
    TDC_PRINTF_E("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE, BUT INVALID T OFFSET LEVEL \r\n");

    tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    tdc_ble_remote_clear_command();
}
else  // 유효한 설정 값인 경우
{
    // 묵음 처리 파일 및 공유 메모리 값 업데이트
    if (tdc_fs_stim_mute_update(TDC_FS_STIM_MUTE_UNDER_T_LEVEL_ENABLE, (uint32_t) remoteDataPacket.data[3]) != TDC_FS_STIM_MUTE_RET_TRUE)
    {
        TDC_PRINTF_E("[MUTE] RECEVIED : WRITE NORMAL + ENABLE MUTE, BUT FAILED TO UPDATE FILE \r\n");

        // 실패 시 에러 전송: 데이터 처리 에러 + 사용할 수 없는 맵데이터
        tdc_sys_error_send_to_app(remoteDataPacket.command, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
        tdc_ble_remote_clear_command();
    }
    else  // 묵음 처리 파일 및 공유 메모리 값 업데이트 성공
    {
        // 송신 데이터 준비
        tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);                                                 //     command : loop-back
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 2);                                                                        //      option : write
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 1);                                                                        // sub option1 : normal mode
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level);  // sub option2 : enable state
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset);            // sub option3 : mute t level offset

        TDC_PRINTF_D("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
        tdc_ble_remote_clear_command();                      // 명령 종료
    }
}
}

/* 0x59 옵션1(읽기) - 현재 묵음 설정 상태를 응답 */
static void tdc_ble_cmd_0x59_sub01_read_state(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     tx_index = 0;

TDC_PRINTF_D("[MUTE] RECEVIED : READ PACKET \r\n");

// 송신 데이터 준비
tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);                                                 //     command : loop-back
tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 1);                                                                        //      option : read
tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 1);                                                                        // sub option1 : normal mode
tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level);  // sub option2 : enable state
tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, (int) cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset);            // sub option3 : mute t level offset

TDC_PRINTF_D("[MUTE] RESPONSE : %02X %02X %02X %02X %02X \r\n", bufferForSPI_tx[0], bufferForSPI_tx[1], bufferForSPI_tx[2], bufferForSPI_tx[3], bufferForSPI_tx[4]);

tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
tdc_ble_remote_clear_command();                      // 명령 종료
}

static void tdc_ble_cmd_0x59_step_system_operation(void)
{
    /* 응답 조립은 전부 하위 함수로 내려갔다. 여기는 분기만 남는다. */
    switch (remoteDataPacket.data[0])  // 옵션
    {
        case 1:  // 읽기 (현재 상태 값으로 응답)
        {
            tdc_ble_cmd_0x59_sub01_read_state();
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
                            tdc_ble_cmd_0x59_sub02_idx01_mute_enable();
                        }
                        break;

                        case 2:  // 묵음 처리 비활성화
                        {
                            tdc_ble_cmd_0x59_sub02_idx02_mute_disable();
                        }
                        break;

                        default:
                        {
                            TDC_PRINTF_E("[MUTE] RECEVIED : WRITE NORMAL OPTION, BUT UNDEFINED SUB OPTION 2 PACKET \r\n");

                            tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                            tdc_ble_remote_clear_command();
                        }
                        break;
                    }  // 끝, switch for 세부 옵션 2
                }
                break;

                case 2:  // 진단 모드 (현재는 N/A)
                {
                    TDC_PRINTF_E("[MUTE] RECEVIED : WRITE DIAGNOSTICS PACKET, BUT N/A CURRENTLY \r\n");

                    tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    tdc_ble_remote_clear_command();
                }
                break;

                default:
                {
                    TDC_PRINTF_E("[MUTE] RECEVIED : UNDEFINED SUB OPTION 1 PACKET \r\n");

                    tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    tdc_ble_remote_clear_command();
                }
                break;
            }  // 끝, switch for 세부 옵션 1
        }
        break;

        default:  // 기타 정의되지 않은 옵션
        {
            TDC_PRINTF_E("[MUTE] RECEVIED : UNDEFINED OPTOIN PACKET \r\n");

            tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            tdc_ble_remote_clear_command();
        }
        break;
    }  // 끝, switch for 옵션

}

/* 0x40 패스키 확인 - 패스키 게이트 밖에서 처리된다.
 * 미인증 상태에서도 응답해야 게이트를 열 수 있기 때문이다. */
static void tdc_ble_cmd_0x40_step_check_passkey(void)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     tx_index = 0;
    bool    remocon_passkey_Match;

    remocon_passkey_Match        = true;

    /* [보안] 리모콘 패스키 인증은 의도적으로 비활성 상태다.
     * 바로 위에서 remocon_passkey_Match 를 무조건 true 로 두므로 어떤 패스키든 통과한다.
     *
     * 원래 이 자리에는 내부기에 저장된 4바이트 패스키
     * (tdc_shm_read_remocon_passkey_connected_isd() 가 돌려준 값)와 리모콘이 보낸
     * remoteDataPacket.data[0..3] 을 비교해 하나라도 다르면 Match 를 false 로 만드는
     * 루프가 있었다. 그 루프가 #if 0 으로 죽어 있어 2차 리팩토링에서 제거했다.
     *
     * 비교 루프가 없어지면서 그 입력이던 패스키 조회
     * (connectedISD_num = tdc_shm_read_connected_isd_num() 후
     *  tdc_shm_read_remocon_passkey_connected_isd(connectedISD_num))도 고아가 되어 함께 지웠다.
     *
     * 인증을 되살리려면 위 두 조회를 되살리고 이 자리에 4바이트 비교를 넣은 뒤
     * 아래 경고 로그를 걷어내면 된다. 공유 메모리 쪽 저장·조회 API 는 그대로 살아 있다. */

    TDC_PRINTF_W("[PASSKEY] ALWAYS PATH OPENED. \r\n");

    // 송신 데이터 준비
    // command loop-back
    tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, remoteDataPacket.command);

    // pay-load 준비
    if (remocon_passkey_Match)
    {
#if 1  // 로그 기능
        tdc_hal_timer_time_t        time;
        TDC_FS_EVENT_LOG_BT_ADDR_T bt_addr;

        // 참조 시간 정보
        time.year  = remoteDataPacket.data[4];
        time.month = remoteDataPacket.data[5];
        time.day   = remoteDataPacket.data[6];
        time.hour  = remoteDataPacket.data[7];
        time.min   = remoteDataPacket.data[8];
        time.sec   = remoteDataPacket.data[9];

        tdc_hal_timer_update_reference_time(&time, 0);

        // 블루투스 주소 정보
        bt_addr.bt_addr[0] = remoteDataPacket.data[10];
        bt_addr.bt_addr[1] = remoteDataPacket.data[11];
        bt_addr.bt_addr[2] = remoteDataPacket.data[12];
        bt_addr.bt_addr[3] = remoteDataPacket.data[13];
        bt_addr.bt_addr[4] = remoteDataPacket.data[14];
        bt_addr.bt_addr[5] = remoteDataPacket.data[15];

        tdc_fs_event_log_update_bt_addr(&bt_addr);

        // 로그 쓰기 : 블루투스 연결
        tdc_fs_event_log_write(TDC_FS_EVENT_LOG_TYPE_CONNECTED);
#endif
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 1);

        // 패스키 매치 업데이트
        tdc_ble_remote_set_passkey_match();
    }
    else
    {
        tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, 2);
    }

    // 송신 데이터 SPI TX버퍼에 복사
    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);

    //  명령 종료
    tdc_ble_remote_clear_command();
}

ST__REMOTECONTROL_STATE tdc_ble_remote_step(bool isdConnection)  // 연결 상태에 변화에 따라 패스키 리셋
{
    /* noCommandTime_counter 제거 (2026-08-06). 무명령 지속 시간을 세던 카운터인데
     * 증가·리셋만 하고 읽는 곳이 한 군데도 없었다(선언 1 + 쓰기 2 + 읽기 0).
     * df_DisConnectionCheckTime(180000) 으로 초기화해 "3분 무명령이면 무언가 한다" 는
     * 의도였을 것이나 그 판정부가 없다. 되살리려면 이 카운터와 임계 비교를 함께
     * 넣어야 한다 - 카운터만 두면 지금처럼 조용히 죽는다.
     * 짝이던 df_DisConnectionCheckTime 도 유일한 사용처가 이것뿐이라 함께 제거했다. */
    static EN__REMOTE_CONTROL_COMMAND prev_remotegCommand = en__remoteControl_IDLE;
    static int                        disconnectionCounter  = 0;
    ST__REMOTECONTROL_STATE           RemoteControlState    = {en__isdStatus_NA, false};

    /* 남은 지역은 이 함수에 그대로 둔 case 들이 쓰는 것뿐이다. 명령별로
     * 추출한 본문이 쓰던 지역(volume · p_connected_isd_usableMapIndex ·
     * p_mapStemp 등)은 각 함수로 함께 옮겨갔다. */
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];  /* 0x8F · 게인제어 (미추출) */
    int     tx_index = 0;                         /* 동 */

    bool remoteCommandStartFlag = false;  /* 플래시 case 들이 그대로 쓴다 */
    bool result;                          /* 0x57 복구 완료 여부 */

    /* 매핑 명령 수신 시점에 내부기 연결확인용 백텔 전송이 진행 중이면, 백텔 수신이 끝난 뒤에
     * 명령을 실행하도록 미루던 로직은 2차 리팩토링에서 제거했다(#if 0 사장).
     * 현재는 이 대기 없이 바로 처리한다. fetched_command 와
     * BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation 을 보던 코드였다. */

    if (!isdConnection)
    {
        disconnectionCounter++;
        if (disconnectionCounter >= 1500)
        {
            tdc_ble_remote_clear_passkey_match();
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
        tdc_ble_cmd_0x40_step_check_passkey();
    }
    else
    {
        if (tdc_ble_remote_is_passkey_match())
        {
            switch (remoteDataPacket.command)
            {
                case en__remoteControl_readInfoOfExtenalDevice:
                {
                    // NRF에서 바로 응답하기 때문에 Ezairo로 명령이 수신되지 않는다.
                }
                break;

                case en__remoteControl_readInfoOfMap:
                {
                    tdc_ble_cmd_0x42_step_read_map_info();
                }
                break;
                /* en__remoteControl_readUserName 핸들러는 2차 리팩토링에서 제거했다(#if 0 사장).
                 * 유저명 조회 커맨드가 폐기돼 CM3 는 이 커맨드에 응답하지 않는다.
                 * 커맨드 코드(en__remoteControl_readUserName)는 프로토콜 열거형에 남아 있다. */
                case en__remoteControl_readStatusOfExtenalDevice:  // 0x43
                {
                    tdc_ble_cmd_0x43_step_read_device_status();
                }
                break;

                case en__remoteControl_changeMapNum:
                {
                    tdc_ble_cmd_0x44_step_change_map_num();
                }
                break;

                case en__remoteControl_adjustStimulationVolume:  // 0x45
                {
                    tdc_ble_cmd_0x45_step_adjust_stim_volume();
                }
                break;

                case en__remoteControl_adjustMicVolume:  // 0x46
                {
                    tdc_ble_cmd_0x46_step_adjust_mic_volume();
                }
                break;

                case en__remoteControl_OnOffTelecoil:  // 0x47
                {
                    tdc_ble_cmd_0x47_step_onoff_telecoil();
                }
                break;

                case en__remoteControl_OnOffStimulationIndicator:  // 0x48
                {
                    tdc_ble_cmd_0x48_step_onoff_stim_indicator();
                }
                break;

                case en__remoteControl_OnOffLED:  // 0x49
                {
                    tdc_ble_cmd_0x49_step_onoff_led();
                }
                break;

                case en__remoteControl_readSoundSignal:
                {
                    tdc_ble_remote_read_sp_para(remoteCommandStartFlag, en__remoteControl_readSoundSignal);
                }
                break;

                case en__remoteControl_read_Ezairo_FirmwareInfo:
                {
                    tdc_ble_cmd_0x54_step_read_firmware_info();
                }
                break;

                case en__remoteControl_read_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_read_info_setting(remoteCommandStartFlag, en__remoteControl_read_SlotData_ISD_N_USER, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__remoteControl_write_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_write_info_setting(remoteCommandStartFlag, en__remoteControl_write_SlotData_ISD_N_USER, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__remoteControl_read_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_read_stim_para(remoteCommandStartFlag, en__remoteControl_read_Mapdata_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__remoteControl_write_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_write_stim_para(remoteCommandStartFlag, en__remoteControl_write_Mapdata_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__remoteControl_erase_SlotData:
                {
                    tdc_isd_map_reset_nvm_selected(remoteCommandStartFlag, en__remoteControl_erase_SlotData, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, flash_Command_Erase);
                }
                break;

                case en__remoteControl_erase_mapData_STIMUL_PARA:
                {
                    tdc_isd_map_reset_nvm_map_data(remoteCommandStartFlag, en__remoteControl_erase_mapData_STIMUL_PARA, remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index, remoteDataPacket.remocon_ReadWriteMapData_Flash.map_index, flash_Command_Erase);
                }
                break;

                case en__remoteControl_erase_mppingData_exceptSlot_1:
                {
                    tdc_isd_map_reset_nvm_2to4(remoteCommandStartFlag, en__remoteControl_erase_mppingData_exceptSlot_1, flash_Command_Erase);
                }
                break;

                case en__remoteControl_recover_ALL_SlotData_ManufactureData:
                {
                    TDC_PRINTF_I("[PACKET] RECEIVED, RECOVER ALL SLOT DATA MANUFACTURE DATA \r\n");

                    result = tdc_isd_map_reset_nvm_all(remoteCommandStartFlag, en__remoteControl_recover_ALL_SlotData_ManufactureData, flash_Command_Recover);

                    if (result)
                    {
                        TDC_PRINTF_I("[PACKET] RESULT : TRUE, AFTER RESET NVM ALL ISD ALL DATA \r\n");

                        while (1)
                        {
                            if (tdc_hal_spi_is_tx_buffer_empty())
                            {
                                // tdc_shm_change_system_mode_flag(en__systemReset);  // 제조용 리모콘에서 페어링키 쓰기 명령을 보낼때 nrf에서 페어링키를 쓰고 리셋을
                                // 한다. 따라서 여기서 리셋하는 경우가 발생하면 전송에러가 발생된다. 명령 종료
                                if (en__remoteControl_recover_ALL_SlotData_ManufactureData > 0x60)
                                {
                                    tdc_ble_mapping_clear_command();
                                }
                                else
                                {
                                    tdc_ble_remote_clear_command();
                                }
                                break;
                            }
                        }

                        // Sys_GPIO_Set_Low(DIO19);
                    }
                }
                break;

                /* en__remoteControl_read_Connected_ISD_id 핸들러는 2차 리팩토링에서 제거했다.
                 * 원 주석: "프로토콜 3.8부터 삭제되었으나 리모콘 에러시 확인 필요".
                 * 연결된 내부기 ID 4바이트를 리모콘에 돌려주던 응답인데, 프로토콜 3.8 이후로는
                 * 리모콘이 이 커맨드를 보내지 않아 #if 0 으로 죽어 있었다.
                 * 커맨드 코드는 프로토콜 열거형에 그대로 있고 tdc_isd_read_connected_id() 도
                 * 살아 있으므로, 디버깅에 다시 필요하면 git 이력에서 이 case 만 복원하면 된다. */

#if 1
                /**
                 * 26.06.23 범용 디버깅 프로토콜 기능에 대한 코드
                 * by 김은수
                 * 26.07.01 tdc_ble_general_debug.c 로 분리 */
                case EN__SND_BT_CMD_GENERAL_DEBUG:
                {
                    tx_index = tdc_ble_general_debug_handle(&remoteDataPacket, bufferForSPI_tx, tx_index);

                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    tdc_ble_remote_clear_command();                      // 명령 종료
                }
                break;
#endif

#if 1
                /**
                 * 26.07.20 Gain Conversion Table 인덱스 설정 프로토콜 (0x8C)
                 * by 김은수 */
                case EN__SND_BT_CMD_GAIN_CONTROL:
                {
                    tx_index = tdc_ble_gain_control_handle(&remoteDataPacket, bufferForSPI_tx, tx_index);

                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    tdc_ble_remote_clear_command();                      // 명령 종료
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
                    tdc_ble_cmd_0x59_step_system_operation();
                }  // 끝, case en__remoteControl_specificSystemOperationSetting:
                break;
#endif

                case en__remoteControl_IDLE :
                {
                }
                break;

            }  // End, "switch (remoteDataPacket.command)"
        }      // End, "if (tdc_ble_remote_is_passkey_match())"
        else
        {
            if(remoteDataPacket.command != en__remoteControl_IDLE)
            {
                // 송신 데이터 SPI TX버퍼에 복사
                tdc_sys_error_send_to_app(remoteDataPacket.command, en__EN__BLE_PROTOCOL_ERROR ,en__NO_SECURITY, __LINE__ );
                //  명령 종료
                tdc_ble_remote_clear_command();
            }
            else
            {
            }
        }
    }

    return RemoteControlState;
}
