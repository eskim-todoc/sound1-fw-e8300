
#include <hw.h>
#include <stdbool.h>
#include <stdint.h>
#include <tdc_ble_map_flash.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_data.h>  // numPacket_writeMapData_* (패킷 분할 개수)
#include <tdc_ble_map_field_range.h>
#include <tdc_ble_reply.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

// 맵 데이터 쓰기(0x6D)에서 여러 패킷에 걸쳐 누적되는 저장소 인덱스.
// 0x68 · 0x6C 는 순서 오류 시 이 값을 0 으로 되돌린다.
static int stimulPara_index = 0;

// 복구 명령(0x70 · 0x71)이 지정하는 쓰기 시작 슬롯.
static int writingStartSlot_index = 0;

void tdc_ble_cmd_0x69_read_original_isd_user(void)  // 0x69
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->fetched_command = en__mapping_read_original_ISD_N_USER;
}

void tdc_ble_cmd_0x68_write_original_isd_user(const uint8_t *Rx_dataPacket)  // 0x68
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  i, k;
    int  subCommandData_Num_index;
    int  tempValue      = 0;
    bool dataRangeError = false;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    int *p_RepositoryFor_ISD_info;

    p_RepositoryFor_ISD_info = tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info();

    subCommandData_Num_index = Rx_dataPacket[index++];

    if (subCommandData_Num_index == 1)
    {
        tdc_ble_mapping_set_seq_index(0);
    }

    if (subCommandData_Num_index != tdc_ble_mapping_get_seq_index() + 1)
    {
        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
        tdc_sys_error_send_to_app(en__mapping_write_original_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);

        tdc_ble_mapping_set_seq_index(0);
        stimulPara_index = 0;
    }
    else
    {
        tdc_ble_mapping_set_seq_index(subCommandData_Num_index);

        switch (subCommandData_Num_index)
        {
            case 1:  // 데이터 인덱스 1
            {
                for (i = 0; i < 18; i++)
                {
                    switch (i)
                    {
                        case 0:  // 내부기 제조번호  년 8bit
                            p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];

                            p_mappingPacket->rx_orignal_ISD_info.ISD_id = p_RepositoryFor_ISD_info[i];
                            p_mappingPacket->rx_orignal_ISD_info.ISD_id = ((p_mappingPacket->rx_orignal_ISD_info.ISD_id) << 8);
                            break;

                        case 1:                                                    // 내부기 제조번호  월 4bit + 모델번호 4bit
                            p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];  // 내부기 제조번호 (년, 월_모델번호)

                            // 실제 내부기 ID 확인용..
                            p_mappingPacket->rx_orignal_ISD_info.ISD_id = p_mappingPacket->rx_orignal_ISD_info.ISD_id | p_RepositoryFor_ISD_info[i];
                            p_mappingPacket->rx_orignal_ISD_info.ISD_id = ((p_mappingPacket->rx_orignal_ISD_info.ISD_id) << 16);

                            tempValue = sizeof(p_mappingPacket->rx_orignal_ISD_info.ISD_id);
                            break;

                        case 2:                                  // 시리얼 번호 상위 8bit
                            tempValue = Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                            tempValue = tempValue << 8;
                            break;

                        case 3:  // 시리얼 번호 하위 8bit
                            // 상위 8bit와 하위 8bit를 합쳐서
                            // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                            p_RepositoryFor_ISD_info[i - 1] = tempValue | Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit

                            // 실제 내부기 ID 확인용..
                            p_mappingPacket->rx_orignal_ISD_info.ISD_id = p_mappingPacket->rx_orignal_ISD_info.ISD_id | p_RepositoryFor_ISD_info[i - 1];
                            break;

                        case 4:  // 수술 위치
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];

                            // 실제 내부기 ID 확인용..
                            p_mappingPacket->rx_orignal_ISD_info.location = p_RepositoryFor_ISD_info[i - 1];

                            if ((p_mappingPacket->rx_orignal_ISD_info.location < 1) || (2 < p_mappingPacket->rx_orignal_ISD_info.location))
                            {
                                dataRangeError = true;
                            }

                            k = 0;
                            break;

                        default:  // 사용자 이니셜
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];

                            // 실제 내부기 ID 확인용..
                            p_mappingPacket->rx_orignal_ISD_info.userName[k] = p_RepositoryFor_ISD_info[i - 1];
                            // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                            k++;
                            break;
                    }
                }
            }
            break;

            case 2:  // 데이터 인덱스 2
            {
                for (i = 18, k = 13; i < 30; k++, i++)
                {
                    p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //  사용자 이름 나머지

                    // 실제 내부기 ID 확인용..
                    p_mappingPacket->rx_orignal_ISD_info.userName[k] = p_RepositoryFor_ISD_info[i - 1];
                }

                p_mappingPacket->rx_orignal_ISD_info.id_check_is_completed = false;
                p_mappingPacket->rx_orignal_ISD_info.id_match              = false;

                //                                                              // 시리얼 번호가 16bit가
                //                                                              전달되면서 인덱스 감소 [i-1]

                // 나머지 데이터는 초기값으로 설정한다.
                // 패스키
                p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;

                // 맵번호
                p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1~4
                // 자극볼륨
                p_RepositoryFor_ISD_info[(i++) - 1] = 4;  // 1~4
                // 마이크감도
                p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1~10
                // LED
                p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔
                // 자극알림
                p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔
                // 텔레코일
                p_RepositoryFor_ISD_info[(i++) - 1] = 2;  // 1 : 켬, 2 : 끔

                // Ble On/Off
                p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔

                // 맵 스템프
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                p_RepositoryFor_ISD_info[(i++) - 1] = 0;
            }
            break;

            default:
            {
            }
            break;
        }

        if (!dataRangeError)
        {
            if (subCommandData_Num_index != numPacket_writeMapData_Original_ISDnSetting)
            {
                buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__mapping_write_original_ISD_N_USER);  // command loop-back
                buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, subCommandData_Num_index);                   // payload num 전송

                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);  // 송신 데이터 SPI TX버퍼에 복사
            }
            else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
            {
                tdc_ble_mapping_set_seq_index(0);

                /* 명령 수신 직후 여기서 곧바로 루프백 응답을 보내던 방식은 제거했다(#if 0 사장).
                 * 원 주석: "매핑 명령 실행 하는 곳에서 최종 응답을 주게 변경하였다".
                 * NRF 광고 이름 변경을 위해 NRF 를 껐다 켜야 해서, 끄기 전에 미리 응답하던
                 * 구조였다. 지금은 명령 실행부가 최종 응답을 보낸다. */

                p_mappingPacket->command = en__mapping_write_original_ISD_N_USER;

                // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
            }
        }
        else
        {
            // 에러 전송, 데이터 범위를 벗어남
            tdc_sys_error_send_to_app(en__mapping_write_original_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        }
    }
}

void tdc_ble_cmd_0x6A_read_slot_data(const uint8_t *Rx_dataPacket)  // 0x6A
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int tempValue;

    tempValue = Rx_dataPacket[index++];
    if ((tempValue >= 1)               // 1 이상
        && (tempValue <= MaxNumUser))  // 4 이하만 유효
    {
        p_mappingPacket->ReadWriteMapData_Flash.slot_index = tempValue;
        p_mappingPacket->ReadWriteMapData_Flash.map_index  = 0;

        p_mappingPacket->fetched_command = en__mapping_read_SlotData_ISD_N_USER;
    }
    else
    {
        // 에러 전송
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_read_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    }
}

void tdc_ble_cmd_0x6C_write_slot_data(const uint8_t *Rx_dataPacket)  // 0x6C
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  i;
    int  subCommandData_Num_index;
    int  tempValue      = 0;
    bool dataRangeError = false;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    int *p_RepositoryFor_ISD_info;

    p_RepositoryFor_ISD_info = tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info();

    subCommandData_Num_index = Rx_dataPacket[index++];

    if (subCommandData_Num_index == 1)
    {
        tdc_ble_mapping_set_seq_index(0);
    }

    if (subCommandData_Num_index != tdc_ble_mapping_get_seq_index() + 1)
    {
        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
        tdc_sys_error_send_to_app(en__mapping_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);

        tdc_ble_mapping_set_seq_index(0);
        stimulPara_index = 0;
    }
    else
    {
        tdc_ble_mapping_set_seq_index(subCommandData_Num_index);

        switch (subCommandData_Num_index)
        {
            case 1:  // 데이터 인덱스 1
            {
                // 슬롯 번호 범위가 맞으면 진행
                tempValue = Rx_dataPacket[index++];
                if ((1 <= tempValue) && (tempValue <= MaxNumUser))
                {
                    p_mappingPacket->ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                    for (i = 0; i < 17; i++)
                    {
                        switch (i)
                        {
                            case 0:
                            case 1:
                                p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];  // 내부기 제조번호 (년, 월_모델번호)
                                break;

                            case 2:                                  // 시리얼 번호 상위 8bit
                                tempValue = Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                                tempValue = tempValue << 8;
                                break;

                            case 3:                                                                    // 시리얼 번호 하위 8bit
                                p_RepositoryFor_ISD_info[i - 1] = tempValue | Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                                // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                break;

                            case 4:  // 수술 위치
                                p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];
                                // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                if ((p_RepositoryFor_ISD_info[i - 1] < 1) || (2 < p_RepositoryFor_ISD_info[i - 1]))
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

            case 2:  // 데이터 인덱스 2
            {
                for (i = 17; i < 35; i++)
                {
                    //  사용자 이름 나머지, 내부기 제어용 passkey, 맵번호, 시리얼
                    p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //  번호가 16bit가 전달되면서 인덱스 감소 [i-1]

                    switch (i)
                    {
                        case 30:  // 내부기 패스키
                        case 31:  // 내부기 패스키
                        case 32:  // 내부기 패스키
                        case 33:  // 내부기 패스키
                            if (!((('0' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= '9'))
                                  || (('a' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 'z'))
                                  || (('A' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 'Z'))))
                            {
                                dataRangeError = true;
                            }
                            break;

                        case 34:  // 맵 번호
                            if (!((1 <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 4)))
                            {
                                dataRangeError = true;
                            }
                            break;
                    }
                }
            }
            break;

            case 3:  // 데이터 인덱스 3
            {
                i = 35;

                // 자극 볼륨
                tempValue = Rx_dataPacket[index++];  // i=35

                if ((1 <= tempValue) && (tempValue <= df_maxStimulationVloumeLevel))
                {
                    p_RepositoryFor_ISD_info[i - 1] = tempValue;
                }
                else
                {
                    dataRangeError = true;
                }

                // 마이크 감도
                i++;  // i=36
                tempValue = Rx_dataPacket[index++];

                if ((1 <= tempValue) && (tempValue <= df_maxMicVloumeLevel))
                {
                    p_RepositoryFor_ISD_info[i - 1] = tempValue;
                }
                else
                {
                    dataRangeError = true;
                }

                // LED 설정
                i++;  // i=37
                tempValue = Rx_dataPacket[index++];

                if ((1 <= tempValue) && (tempValue <= 2))
                {
                    p_RepositoryFor_ISD_info[i - 1] = tempValue;
                }
                else
                {
                    dataRangeError = true;
                }

                // 자극 알림 설정
                i++;  // i=38
                tempValue = Rx_dataPacket[index++];

                if ((1 <= tempValue) && (tempValue <= 2))
                {
                    p_RepositoryFor_ISD_info[i - 1] = tempValue;
                }
                else
                {
                    dataRangeError = true;
                }

                // telecoil 설정
                i++;  // i=39
                tempValue = Rx_dataPacket[index++];

                // 1세대 적용된 마지막 매핑 프로토콜 v1.0.5 기준으로는
                // 텔레코일 값 범위가 1에서 2로 되어 있지만,
                // 실제 운용되는 리모콘 및 매핑 앱에서는 텔레코일을 사용하지 않기 때문에
                // 0 값을 전달한다. 이로 인해 데이터 범위 에러가 발생한다.
                // 0에서 2까지 범위를 허용하도록 잠수함 패치한다. (2026.02.23 by 김은수)
                if ((0 /*1*/ <= tempValue) && (tempValue <= 2))
                {
                    p_RepositoryFor_ISD_info[i - 1] = tempValue;
                }
                else
                {
                    dataRangeError = true;
                }
                /* 텔레코일 설정은 현재 버전에서 강제 비활성이다(tdc_ble_remote.c 와 동일 처리).
                 * 리모콘이 보낸 tempValue 를 쓰지 않고 항상 2(꺼짐)를 저장한다.
                 * 원래 코드는 `= tempValue;` 였고 #if 0 으로 죽어 있어 정리했다. */
                p_RepositoryFor_ISD_info[i - 1] = 2;
                //  자극 볼륨, 마이크 감도, LED 설정, 자극 알림 설정, 텔레 코일 설정,

                // BLE On/OFF 옵션
                i++;                                  // i=40
                p_RepositoryFor_ISD_info[i - 1] = 1;  // BLE On/OFF 옵션 (패킷 프로토콜에는 없음. 항상 1로 설정시키게 함)

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
                buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__mapping_write_SlotData_ISD_N_USER);  // command loop-back
                buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, subCommandData_Num_index);                   // payload num 전송

                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);  // 송신 데이터 SPI TX버퍼에 복사
            }
            else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
            {
                tdc_ble_mapping_set_seq_index(0);
                p_mappingPacket->command = en__mapping_write_SlotData_ISD_N_USER;

                // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
            }
        }
        else
        {
            // 에러 전송
            // 데이터 범위를 벗어남
            tdc_sys_error_send_to_app(en__mapping_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        }
    }
}

void tdc_ble_cmd_0x6B_read_map_data(const uint8_t *Rx_dataPacket)  // 0x6B
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int tempValue;

    tempValue = Rx_dataPacket[index++];

    if ((1 <= tempValue) && (tempValue <= MaxNumUser))
    {
        p_mappingPacket->ReadWriteMapData_Flash.slot_index = tempValue;
        tempValue                                          = Rx_dataPacket[index++];

        if ((tempValue >= 1) && (tempValue <= MaxNumMap))
        {
            p_mappingPacket->ReadWriteMapData_Flash.map_index = tempValue;
            p_mappingPacket->fetched_command                  = en__mapping_read_Mapdata_STIMUL_PARA;
        }
        else
        {
            // 에러 전송
            // 데이터 범위를 벗어남
            tdc_sys_error_send_to_app(en__mapping_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        }
    }
    else
    {
        // 에러 전송
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    }
}

void tdc_ble_cmd_0x6D_write_map_data(const uint8_t *Rx_dataPacket)  // 0x6D
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  i;
    int  subCommandData_Num_index;
    int  tempValue = 0;
    int  intFromByte;
    bool dataRangeError = false;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    int *p_RepositoryFor_stimulPara;

    p_RepositoryFor_stimulPara = tdc_shm_get_pointer_repository_for_read_write_map_data_stimul_para();

    subCommandData_Num_index = Rx_dataPacket[index++];
    if (subCommandData_Num_index == 1)
    {
        tdc_ble_mapping_set_seq_index(0);
    }

    if (subCommandData_Num_index != tdc_ble_mapping_get_seq_index() + 1)
    {
        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
        tdc_sys_error_send_to_app(en__mapping_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);
        tdc_ble_mapping_set_seq_index(0);
    }
    else
    {
        tdc_ble_mapping_set_seq_index(subCommandData_Num_index);

        switch (subCommandData_Num_index)
        {
            case 1:  // 데이터 인덱스 1
            {
                // 슬롯 범위가 맞아야 진입
                tempValue = Rx_dataPacket[index++];
                if ((1 <= tempValue) && (tempValue <= MaxNumUser))
                {
                    p_mappingPacket->ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                    tempValue = Rx_dataPacket[index++];

                    // 프로그램 범위가 맞아야 진입
                    if ((tempValue >= 1) && (tempValue <= MaxNumMap))
                    {
                        p_mappingPacket->ReadWriteMapData_Flash.map_index = tempValue;  // 프로그램 번호

                        stimulPara_index = 0;
                        for (i = 0; i < TDC_BLE_MAP_FIELD_STIMUL_PARA_NUM; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 매핑 일자, 자극 펄스 파라미터 들..

                            if (!tdc_ble_map_field_stimul_para_in_range(i, p_RepositoryFor_stimulPara[stimulPara_index]))
                            {
                                dataRangeError = true;
                            }

                            stimulPara_index++;
                        }

                        intFromByte                                    = Rx_dataPacket[index++] << 8;           // 자극 알람 크기, 상위 바이트
                        intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                        p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;

                        if (!tdc_ble_map_field_level_uA_in_range(intFromByte))  // 하한 0 은 26.02.25 김은수 수정분
                        {
                            dataRangeError = true;
                        }
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
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 사용가능 전극 번호 18개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
                }
            }
            break;

            case 3:
            {
                for (i = 0; i < 14; i++)
                {
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 사용가능 전극 번호 12개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
                }
            }
            break;

            case 4:
            {
                for (i = 0; i < 18; i++)
                {
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 18개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
                }
            }
            break;

            case 5:
            {
                for (i = 0; i < 14; i++)
                {
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 12개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
                }
            }
            break;

            case 6:
            {
                for (i = 0; i < 18; i++)
                {
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 18개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
                }
            }
            break;

            case 7:
            {
                for (i = 0; i < 14; i++)
                {
                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 12개
                    if (!tdc_ble_map_field_electrode_in_range(p_RepositoryFor_stimulPara[stimulPara_index]))
                    {
                        dataRangeError = true;
                    }
                    stimulPara_index++;
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

                    if (!tdc_ble_map_field_level_uA_in_range(intFromByte))
                    {
                        dataRangeError = true;
                    }
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

                    if (!tdc_ble_map_field_level_uA_in_range(intFromByte))
                    {
                        dataRangeError = true;
                    }
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
                buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__mapping_write_Mapdata_STIMUL_PARA);

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

                tdc_ble_mapping_set_seq_index(0);
                stimulPara_index = 0;

                p_mappingPacket->fetched_command = en__mapping_write_Mapdata_STIMUL_PARA;

                // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
            }
        }
        else
        {
            tdc_sys_error_send_to_app(en__mapping_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
        }
    }
}

void tdc_ble_cmd_0x6E_erase_slot(int command, const uint8_t *Rx_dataPacket)  // 0x6E
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;

    p_mappingPacket->fetched_command                   = command;
    p_mappingPacket->ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];

    if (!((1 <= p_mappingPacket->ReadWriteMapData_Flash.slot_index) && (p_mappingPacket->ReadWriteMapData_Flash.slot_index <= 4)))
    {
        tdc_sys_error_send_to_app(en__mapping_erase_SlotData_manufacture, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
        p_mappingPacket->fetched_command = en__mapping_IDLE;
    }
}

void tdc_ble_cmd_0x6F_erase_map(int command, const uint8_t *Rx_dataPacket)  // 0x6F
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;

    p_mappingPacket->fetched_command                   = command;
    p_mappingPacket->ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
    p_mappingPacket->ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];

    if (!((1 <= p_mappingPacket->ReadWriteMapData_Flash.slot_index) && (p_mappingPacket->ReadWriteMapData_Flash.slot_index <= 4)))
    {
        tdc_sys_error_send_to_app(en__mapping_erase_mapData_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
        p_mappingPacket->fetched_command = en__mapping_IDLE;
    }

    if (!((1 <= p_mappingPacket->ReadWriteMapData_Flash.map_index) && (p_mappingPacket->ReadWriteMapData_Flash.map_index <= 4)))
    {
        tdc_sys_error_send_to_app(en__mapping_erase_mapData_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
        p_mappingPacket->fetched_command = en__mapping_IDLE;
    }
}

void tdc_ble_cmd_0x70_recover_except_slot1(int command, const uint8_t *Rx_dataPacket)  // 0x70
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;

    writingStartSlot_index                             = 2;
    p_mappingPacket->fetched_command                   = command;
    p_mappingPacket->ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
    p_mappingPacket->ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
}

void tdc_ble_cmd_0x71_recover_all(int command)  // 0x71
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    writingStartSlot_index           = 1;
    p_mappingPacket->fetched_command = command;
}
