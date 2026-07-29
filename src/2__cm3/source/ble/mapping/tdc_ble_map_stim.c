
#include <hw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tdc_ble_map_stim.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_isd.h>
#include <tdc_isd_map_data.h>
#include <tdc_printf.h>
#include <tdc_ble_reply.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

void tdc_ble_map_stim_specific(const uint8_t *Rx_dataPacket)  // 0x65
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  value;
    bool dataRangeError = false;

    p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum           = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth                   = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase              = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode               = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum      = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];

    value                                                     = Rx_dataPacket[index++] << 8;
    value                                                     = value | Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA     = value;
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec = Rx_dataPacket[index++];

    // 데이터 범위 검사
    dataRangeError = false;

    // 사용 가능한 전극 수
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 펄스 위상 폭
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth < 13)       // 13 미만
        || (255 < p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 선행 펄스 위상
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase < 0)      // 0 미만
        || (1 < p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase))  // 1 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 모드
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode < 1)      // 1 미만
        || (6 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode))  // 6 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 전극 번호
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 바이폴라 모드일 때, 기준전극 번호
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum))  //  32 초과 시 에러
    {
        if (p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
        {
            dataRangeError = true;
        }
    }

    // 자극 크기 uA
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA < 0 /*1*/)   // 0 미만
        || (1800 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA))  // 1800 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 유지 시간 100msec
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec < 1)        // 1 미만
        || (255 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    if (dataRangeError)
    {
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    }
    else
    {
        p_mappingPacket->fetched_command = en__mapping_specific_stimulation;
    }
}

// ---------------------------------------------------------------------------
// 0x66 하위 1 (전체 파라미터) 패킷 디스크립터
//
// 20바이트 MTU 제약 때문에 파라미터가 데이터 인덱스 1~15 로 쪼개져 도착한다.
// 인덱스마다 "이 위치에 이 필드가 이 폭 · 이 범위로 온다"를 데이터로 선언하고,
// 파싱은 테이블을 훑는 단일 루프가 담당한다.
//
// 프로토콜 문서 대조 : docs/참고/ble/프로토콜/명령 카탈로그.md 의 0x66 항목
// ---------------------------------------------------------------------------

#define TDC_LIVE_T ST__MAPPINGPAYLOAD_LIVE_STIMULATION

typedef struct
{
    uint16_t dst_offset;  // 페이로드 구조체 기준 바이트 오프셋
    int16_t  min;         // 이 값 미만이면 범위 에러
    int16_t  max;         // 이 값 초과면 범위 에러
    uint8_t  dst_size;    // 대상 필드 폭 - 1 또는 4
    uint8_t  src_width;   // 패킷에서 읽는 폭 - 1 또는 2 (2는 상위 바이트 먼저)
    uint8_t  count;       // 연속 원소 수 - 스칼라 1, 배열 청크 8 / 15 / 17
} tdc_ble_field_desc_t;

// dst_size 를 상수로 적지 않고 sizeof 로 뽑는 이유.
//
// stimulationMode 만 EN___STIMULATION_MODE 이고, ARM EABI 는 -fshort-enums 가
// 기본이라 이 필드가 1바이트다. 반면 오프라인 테스트를 돌리는 호스트
// 컴파일러에서는 4바이트다. 폭을 손으로 적으면 호스트 테스트는 통과하고
// 실기에서만 인접 바이트를 덮는다. 컴파일러에게 맡기면 양쪽 다 정확하다.
#define TDC_BLE_DESC_SCALAR(f, srcw, mn, mx)     \
    {                                            \
        offsetof(TDC_LIVE_T, f),                 \
        (mn),                                    \
        (mx),                                    \
        (uint8_t) sizeof(((TDC_LIVE_T *) 0)->f), \
        (srcw),                                  \
        1                                        \
    }

#define TDC_BLE_DESC_ARRAY(f, start, n, srcw, mn, mx)                           \
    {                                                                           \
        offsetof(TDC_LIVE_T, f) + ((start) * sizeof(((TDC_LIVE_T *) 0)->f[0])), \
        (mn),                                                                   \
        (mx),                                                                   \
        (uint8_t) sizeof(((TDC_LIVE_T *) 0)->f[0]),                             \
        (srcw),                                                                 \
        (n)                                                                     \
    }

static const tdc_ble_field_desc_t s_live_all_param_fields[] = {
    // ---- 데이터 인덱스 1 : 스칼라 9필드 ----
    TDC_BLE_DESC_SCALAR(stimulVolume, 1, 1, 4),                         // 자극 볼륨
    TDC_BLE_DESC_SCALAR(audioVolume, 1, 1, 10),                         // 오디오 볼륨
    TDC_BLE_DESC_SCALAR(stimulationIndicatorChannelNum, 1, 1, 32),      // 알림용 자극 채널 번호
    TDC_BLE_DESC_SCALAR(stimulationIndicatorAmplitude_uA, 2, 0, 1800),  // 알림용 자극 크기 uA
    TDC_BLE_DESC_SCALAR(stimulationStrategy, 1, 1, 3),                  // 자극 기법
    TDC_BLE_DESC_SCALAR(stimulationMode, 1, 1, 6),                      // 자극 모드
    TDC_BLE_DESC_SCALAR(firstPulsePhase, 1, 0, 1),                      // 선행 펄스 위상
    TDC_BLE_DESC_SCALAR(stimulationPulsePhaseWidth, 1, 13, 255),        // 펄스 위상 폭
    TDC_BLE_DESC_SCALAR(numFrequencyBand, 1, 1, 32),                    // 주파수 밴드 수

    // ---- 데이터 인덱스 2~7 : u8 배열 청크 ----
    TDC_BLE_DESC_ARRAY(usableStimulationElectrodIndex, 0, 17, 1, 1, 100),   // 2  자극 전극 [0..16]
    TDC_BLE_DESC_ARRAY(usableStimulationElectrodIndex, 17, 15, 1, 1, 100),  // 3  자극 전극 [17..31]
    TDC_BLE_DESC_ARRAY(usableReferenceElectrodIndex, 0, 17, 1, 1, 100),     // 4  기준 전극 [0..16]
    TDC_BLE_DESC_ARRAY(usableReferenceElectrodIndex, 17, 15, 1, 1, 100),    // 5  기준 전극 [17..31]
    TDC_BLE_DESC_ARRAY(CIS_FreqBandOrder, 0, 17, 1, 1, 100),                // 6  밴드 출력 순서 [0..16]
    TDC_BLE_DESC_ARRAY(CIS_FreqBandOrder, 17, 15, 1, 1, 100),               // 7  밴드 출력 순서 [17..31]

    // ---- 데이터 인덱스 8~15 : u16 배열 청크 ----
    TDC_BLE_DESC_ARRAY(T_level_uA, 0, 8, 2, 0, 1800),   // 8  T 레벨 [0..7]
    TDC_BLE_DESC_ARRAY(T_level_uA, 8, 8, 2, 0, 1800),   // 9  T 레벨 [8..15]
    TDC_BLE_DESC_ARRAY(T_level_uA, 16, 8, 2, 0, 1800),  // 10 T 레벨 [16..23]
    TDC_BLE_DESC_ARRAY(T_level_uA, 24, 8, 2, 0, 1800),  // 11 T 레벨 [24..31]
    TDC_BLE_DESC_ARRAY(C_level_uA, 0, 8, 2, 0, 1800),   // 12 C 레벨 [0..7]
    TDC_BLE_DESC_ARRAY(C_level_uA, 8, 8, 2, 0, 1800),   // 13 C 레벨 [8..15]
    TDC_BLE_DESC_ARRAY(C_level_uA, 16, 8, 2, 0, 1800),  // 14 C 레벨 [16..23]
    TDC_BLE_DESC_ARRAY(C_level_uA, 24, 8, 2, 0, 1800),  // 15 C 레벨 [24..31]
};

// 데이터 인덱스 N 은 s_live_all_param_fields[first] 부터 count 개를 쓴다.
typedef struct
{
    uint8_t first;
    uint8_t count;
} tdc_ble_desc_slot_t;

static const tdc_ble_desc_slot_t s_live_all_param_slots[Live_AllParameter_payloadNum] = {
    {0, 9},   // 1  전체 파라미터 스칼라
    {9, 1},   // 2  자극 전극 [0..16]
    {10, 1},  // 3  자극 전극 [17..31]
    {11, 1},  // 4  기준 전극 [0..16]
    {12, 1},  // 5  기준 전극 [17..31]
    {13, 1},  // 6  밴드 출력 순서 [0..16]
    {14, 1},  // 7  밴드 출력 순서 [17..31]
    {15, 1},  // 8  T 레벨 [0..7]
    {16, 1},  // 9  T 레벨 [8..15]
    {17, 1},  // 10 T 레벨 [16..23]
    {18, 1},  // 11 T 레벨 [24..31]
    {19, 1},  // 12 C 레벨 [0..7]
    {20, 1},  // 13 C 레벨 [8..15]
    {21, 1},  // 14 C 레벨 [16..23]
    {22, 1},  // 15 C 레벨 [24..31]
};

_Static_assert(sizeof(int) == 4, "디스크립터는 int 4바이트를 전제한다");
_Static_assert(sizeof(s_live_all_param_fields) / sizeof(s_live_all_param_fields[0]) == 23,
               "디스크립터 개수가 23 이 아니다");
_Static_assert(sizeof(s_live_all_param_slots) / sizeof(s_live_all_param_slots[0]) == Live_AllParameter_payloadNum,
               "슬롯 테이블 길이가 payloadNum 과 다르다");

// 디스크립터 목록대로 패킷을 파싱해 dst_base 에 적재한다.
//
// 범위를 벗어난 값도 일단 적재한다(기존 동작). 위반이 있어도 중단하지 않고
// 끝까지 돌며 플래그만 세운다 - 첫 위반에서 빠져나오면 구조체에 남는 값이
// 기존과 달라진다.
//
// 반환 : 범위 위반이 하나라도 있으면 true
static bool tdc_ble_desc_parse(void *dst_base, const tdc_ble_field_desc_t *desc, int desc_count, const uint8_t *packet, int index)
{
    bool rangeError = false;
    int  d;
    int  e;

    for (d = 0; d < desc_count; d++)
    {
        const tdc_ble_field_desc_t *f = &desc[d];

        for (e = 0; e < f->count; e++)
        {
            int      value;
            uint8_t *dst = (uint8_t *) dst_base + f->dst_offset + (e * f->dst_size);

            if (f->src_width == 2)
            {
                value = packet[index++] << 8;
                value = value | packet[index++];
            }
            else
            {
                value = packet[index++];
            }

            if (f->dst_size == 1)
            {
                *dst = (uint8_t) value;
            }
            else
            {
                *(int *) dst = value;
            }

            if ((value < f->min) || (f->max < value))
            {
                rangeError = true;
            }
        }
    }

    return rangeError;
}

static void tdc_ble_map_stim_live_all_parameter(const uint8_t *Rx_dataPacket, int index)  // 하위 명령 - en__allParameter
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  i;
    int  value;
    int  subCommandData_Num_index;
    bool dataRangeError = false;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;

    subCommandData_Num_index = Rx_dataPacket[index++];  // index++ : 2 → 3

    if (subCommandData_Num_index == 1)
    {
        tdc_ble_mapping_set_seq_index(0);
    }

    if (subCommandData_Num_index != tdc_ble_mapping_get_seq_index() + 1)
    {
        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
        tdc_ble_mapping_set_seq_index(0);
    }
    else
    {
        tdc_ble_mapping_set_seq_index(subCommandData_Num_index);

        // 라이브 모드:0x66 -> 하위 명령 1: 전체 파라미터 전달 -> 데이터 인덱스 1~15에 대한 switch
        switch (subCommandData_Num_index)
        {
            case 1:  // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 1
            {
                p_mappingPacket->tdc_isd_map_live_step.stimulVolume                     = Rx_dataPacket[index++];          // index++ : 3 → 4
                p_mappingPacket->tdc_isd_map_live_step.audioVolume                      = Rx_dataPacket[index++];          // index++ : 4 → 5
                p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum   = Rx_dataPacket[index++];          // index++ : 5 → 6
                value                                                          = Rx_dataPacket[index++] << 8;     // index++ : 6 → 7
                value                                                          = value | Rx_dataPacket[index++];  // index++ : 7 → 8
                p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;

                p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy        = Rx_dataPacket[index++];  // index++ : 8 → 9
                p_mappingPacket->tdc_isd_map_live_step.stimulationMode            = Rx_dataPacket[index++];  // index++ : 9 → 10
                p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase            = Rx_dataPacket[index++];  // index++ : 10 → 11
                p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth = Rx_dataPacket[index++];  // index++ : 11 → 12
                p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand           = Rx_dataPacket[index++];  // index++ : 12 → 13

                // 자극 볼륨 (p_mappingPacket->tdc_isd_map_live_step.stimulVolume : 1~4)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulVolume < 1)      // 1 미만
                    || (4 < p_mappingPacket->tdc_isd_map_live_step.stimulVolume))  // 4 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 오디오 볼륨 (p_mappingPacket->tdc_isd_map_live_step.audioVolume : 1~10)
                if ((p_mappingPacket->tdc_isd_map_live_step.audioVolume < 1)       // 1 미만
                    || (10 < p_mappingPacket->tdc_isd_map_live_step.audioVolume))  // 10 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 알림용 자극 채널 번호 (p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum : 1~32)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum < 1)       // 1 미만
                    || (32 < p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum))  // 32 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 알림용 자극 크기 uA (p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA : 1~1800)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA < 0 /*1*/)   // 0 미만
                    || (1800 < p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA))  // 1800 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 자극 기법 (p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy : 1~3)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy < 1)      // 1 미만
                    || (3 < p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy))  // 3 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 자극 모드 (p_mappingPacket->tdc_isd_map_live_step.stimulationMode : 1~6)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulationMode < 1)      // 1 미만
                    || (6 < p_mappingPacket->tdc_isd_map_live_step.stimulationMode))  // 6 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 선행 펄스 위상 (p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase : 0~1)
                if ((p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase < 0)      // 0 미만
                    || (1 < p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase))  // 1 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 펄스 위상 폭 (p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth : 13~255)
                if ((p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth < 13)       // 13 미만
                    || (255 < p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth))  // 255 초과 시 에러
                {
                    dataRangeError = true;
                }

                // 주파수 밴드 (p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand : 1~32)
                if ((p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand < 1)       // 1 미만
                    || (32 < p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand))  // 32 초과 시 에러
                {
                    dataRangeError = true;
                }
            }
            break;

            default:
            {
                // 데이터 인덱스 2~15 는 디스크립터 테이블이 처리한다.
                //
                // 범위 밖도 여기로 온다. 데이터 인덱스 카운터를 map_flash 와
                // 공유하는데 flash 는 34 까지 쓰므로 16 이상이 실제로 도달한다.
                // 테이블을 인덱싱하기 전에 반드시 걸러야 한다.
                if ((subCommandData_Num_index < 2) || (Live_AllParameter_payloadNum < subCommandData_Num_index))
                {
                    dataRangeError = true;
                }
                else
                {
                    int slot = subCommandData_Num_index - 1;

                    dataRangeError = tdc_ble_desc_parse(&p_mappingPacket->tdc_isd_map_live_step,
                                                        &s_live_all_param_fields[s_live_all_param_slots[slot].first],
                                                        s_live_all_param_slots[slot].count,
                                                        Rx_dataPacket,
                                                        index);
                }
            }
            break;
        }

        if (dataRangeError)
        {
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
            tdc_ble_mapping_set_seq_index(0);
        }
        else
        {
            // command loop-back
            buffer_tx_index = tdc_ble_reply_header(bufferForSPI_tx, buffer_tx_index, en__mapping_live_stimulation);

            //  sub-command loop-back
            buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, en__allParameter);

            buffer_tx_index = tdc_ble_reply_u8(bufferForSPI_tx, buffer_tx_index, subCommandData_Num_index);  // payload num 전송

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            if (subCommandData_Num_index == Live_AllParameter_payloadNum)  // 모든 파라미터 수신 완료.
            {
                tdc_ble_mapping_set_seq_index(0);

                // 현재는 매핑프로그램에서 받지는 않지만, 펌웨어는 이 값을 사용하여 자극 범위를 계산하고
                // 있다. 매핑 프로그램의 기능이 확장되면 이 값을 넣을 지도 고려..

                // x- min
                for (i = 0; i < 32; i++)
                {
                    p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i] = df_minAudioForLogarithm;
                }

                // x- max
                for (i = 0; i < 32; i++)
                {
                    p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i] = df_maxAudioForLogarithm;
                }

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__allParameter;

                TDC_PRINTF_I("[LIVE] LIVE ALL PARAMETER PAYLOAD NUM : NOW, SUB COMMAND SET TO EN__ALL_PARAMETER \r\n");
            }
            else
            {
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
    }
}

static void tdc_ble_map_stim_live_start(void)  // 하위 명령 - en__Start
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Start;
}

static void tdc_ble_map_stim_live_volume_adjust(const uint8_t *Rx_dataPacket, int index)  // 하위 명령 - en__StimulationVolumeAdjust
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int value;

    if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
    {
        value = Rx_dataPacket[index++];

        if ((1 <= value)                                 // 1 이상
            && (value <= df_maxStimulationVloumeLevel))  // 4 이하
        {
            p_mappingPacket->tdc_isd_map_live_step.stimulVolume = value;
            p_mappingPacket->tdc_isd_map_live_step.subCommand   = en__StimulationVolumeAdjust;
        }
        else
        {
            // 에러 전송
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    }
}

static void tdc_ble_map_stim_live_mic_sensitivity(const uint8_t *Rx_dataPacket, int index)  // 하위 명령 - en__MicSensitivityAdjust
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int value;

    if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
    {
        value = Rx_dataPacket[index++];
        if ((1 <= value)                         // 1 이상
            && (value <= df_maxMicVloumeLevel))  // 10 이하
        {
            p_mappingPacket->tdc_isd_map_live_step.audioVolume = value;
            p_mappingPacket->tdc_isd_map_live_step.subCommand  = en__MicSensitivityAdjust;
        }
        else
        {
            // 에러 전송
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어
        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    }
}

static void tdc_ble_map_stim_live_indicator(const uint8_t *Rx_dataPacket, int index)  // 하위 명령 - en__mapping_Stimul_indicator
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int i;
    int value;

    // 알림 자극의 자극 크기 상·하한. 맵 데이터를 훑어 갱신하며 호출 간에
    // 값을 유지해야 하므로 static 이다(원본 fetch_packet 과 동일).
    static int max_C_uA = 0, min_T_uA = 1800;

    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;

    if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
    {
        p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

        value = Rx_dataPacket[index++];  // 알림용 자극 채널 번호

        if ((value > p_mapDataSharedMemory->numFrequencyBand)  // 알림용 자극 채널 번호가 주파수 밴드 수 보다 크거나
            || (value < 1))                                    // 1 미만이면 에러
        {
            // 에러 전송
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
        else
        {
            p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum = value;

            value = Rx_dataPacket[index++] << 8;     // 알림용 자극 크기 uA (upper)
            value = value | Rx_dataPacket[index++];  // 알림용 자극 크기 uA (lower)

            // 자극 알림 출력값의 데이터 범위 검사를 위한 자극 범위 계산
            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                if (p_mapDataSharedMemory->C_level_uA[i] > max_C_uA)
                {
                    max_C_uA = p_mapDataSharedMemory->C_level_uA[i];
                }

#if 1  // 1세대에서 사용되던 코드 (min_T_uA가 1800으로 유지되는 버그 있음)
                if (p_mapDataSharedMemory->T_level_uA[i] != 0)
                {
                    if (p_mapDataSharedMemory->T_level_uA[i] < min_T_uA)
                    {
                        min_T_uA = p_mapDataSharedMemory->T_level_uA[i];
                    }
                }
#endif
            }

#if 1
            // 0x66 라이브모드의 하위 명령 1, 데이터 인덱스 1에서 주파수 밴드 수를 입력 받았다.
            // 또한, T, C 정보는 사용가능한 전극 번호의 값과는 상관 없이
            // 매핑 앱의 첫번째 채널에서부터 활성화된 수만큼 앞으로 패딩되어 전달된다.
            // 즉, 실제 사용 가능한 밴드 수 만큼만 T레벨을 확인하면
            // 활성화된 밴드들 중에서의 가장 작은 T레벨을 찾을 수 있다.
            // 그게 비로 0 값이라 하더라도, 해당 밴드는 활성화된 밴드이므로 0이 올바른 T레벨 값일 것이다.
            for (int li = 0; li < p_mapDataSharedMemory->numFrequencyBand; li++)
            {
                if (p_mapDataSharedMemory->T_level_uA[li] < min_T_uA)
                {
                    min_T_uA = p_mapDataSharedMemory->T_level_uA[li];
                }
            }
#endif

            if ((value > max_C_uA) || (value < min_T_uA))
            {
                TDC_PRINTF_E("[MAPPING] VALUE=%d, max_C_uA=%d, min_T_uA=%d \r\n", value, max_C_uA, min_T_uA);
                // 에러 전송
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
            else
            {
                p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;
                p_mappingPacket->tdc_isd_map_live_step.subCommand                       = en__mapping_Stimul_indicator;
            }
        }
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    }
}

static void tdc_ble_map_stim_live_read_equalizer(const uint8_t *Rx_dataPacket, int index)  // 하위 명령 - en__readEqualizer
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  tempA, tempB;
    bool dataRangeError = false;

    if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
    {
        tempA = Rx_dataPacket[index++];  // start index
        tempB = Rx_dataPacket[index++];  // end index

        if ((tempA < 1)       // 1 미만
            || (32 < tempA))  // 32 초과 시 에러
        {
            dataRangeError = true;
        }

        if ((tempB < 1)       // 1 미만
            || (32 < tempB))  // 32 초과 시 에러
        {
            dataRangeError = true;
        }

        if (dataRangeError)
        {
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
        else
        {
            if ((tempB - tempA) > 8)
            {
                // 에러 전송
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
            else
            {
                p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadStart_index = tempA;
                p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadEnd_index   = tempB;

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__readEqualizer;
            }
        }
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    }
}

static void tdc_ble_map_stim_live_read_device_status(void)  // 하위 명령 - en__readDeviceStatus
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__readDeviceStatus;
}

static void tdc_ble_map_stim_live_stop(void)  // 하위 명령 - en__Stop
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Stop;
}

void tdc_ble_map_stim_live(const uint8_t *Rx_dataPacket)  // 0x66
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int liveSubCommand;

    // index 1은 하위 명령 1~9에 대한 인덱스
    liveSubCommand = Rx_dataPacket[index++];  // index = 1, index++ = 2

    // liveSubCommand
    // 1: 전체 파라미터 전달             : en__allParameter
    // 2: 라이브 모드 시작               : en__Start
    // 3: 자극 볼륨 조절                 : en__StimulationVolumeAdjust
    // 4: 마이크 감도 조절               : en__MicSensitivityAdjust
    // 5: 알림 자극 출력                 : en__mapping_Stimul_indicator
    // 6: 자극 출력 값 읽기 (이퀄라이저) : en__readEqualizer
    // 7: 장치 상태 읽기                 : en__readDeviceStatus
    // 8: 라이브 모드 종료               : en__Stop

    // 프로토콜과 별개로 사용되는 상태 값
    // 0: en__Standby
    // 9: en__HoldOn

    if ((liveSubCommand < 1) || (9 < liveSubCommand))  // 하위 명령 1~9 가능
    {
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        return;
    }
    else
    {
        p_mappingPacket->fetched_command = en__mapping_live_stimulation;
    }

    switch (liveSubCommand)  // 하위 명령 1~9
    {
        case en__allParameter:  // 하위 명령 1
        {
            tdc_ble_map_stim_live_all_parameter(Rx_dataPacket, index);
        }
        break;

        case en__Start:  // 하위 명령 2 (실시간 자극 시작)
        {
            tdc_ble_map_stim_live_start();
        }
        break;

        case en__StimulationVolumeAdjust:  // 하위 명령 3 (자극 볼륨 조절)
        {
            tdc_ble_map_stim_live_volume_adjust(Rx_dataPacket, index);
        }
        break;

        case en__MicSensitivityAdjust:  // 하위 명령 4 (마이크 감도 조절)
        {
            tdc_ble_map_stim_live_mic_sensitivity(Rx_dataPacket, index);
        }
        break;

        case en__mapping_Stimul_indicator:  // 하위 명령 5 (알림 자극 출력)
        {
            tdc_ble_map_stim_live_indicator(Rx_dataPacket, index);
        }
        break;

        case en__readEqualizer:  // 하위 명령 6 (자극 출력 값 읽기: 이퀄라이저)
        {
            tdc_ble_map_stim_live_read_equalizer(Rx_dataPacket, index);
        }
        break;

        case en__readDeviceStatus:  // 하위 명령 7 (장치 상태 읽기)
        {
            tdc_ble_map_stim_live_read_device_status();
        }
        break;

        case en__Stop:  // 하위 명령 8 (실시간 자극 종료)
        {
            tdc_ble_map_stim_live_stop();
        }
        break;

        default:
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
        break;
    }
}
