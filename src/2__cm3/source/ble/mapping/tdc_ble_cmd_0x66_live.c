
#include <hw.h>
#include <stdbool.h>
#include <stdint.h>
#include <tdc_ble_cmd_0x66_live.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd.h>
#include <tdc_isd_map_data.h>
#include <tdc_printf.h>
#include <tdc_ble_reply.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 파싱은 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

// ===========================================================================
// 하위 명령 1 (전체 파라미터) - 데이터 인덱스 1~15
//
// 각 함수는 자기 데이터 인덱스의 페이로드만 파싱해 적재하고, 범위 위반
// 여부를 반환한다. 위반이 있어도 중간에 빠져나오지 않고 끝까지 적재한다
// (원본 동작 - 조기 종료하면 구조체에 남는 값이 달라진다).
//
// 패킷 인덱스는 3 부터다. 0 = 명령(0x66), 1 = 하위 명령(1), 2 = 데이터 인덱스.
// ===========================================================================

// 데이터 인덱스 1 - 자극·오디오 볼륨, 알림 자극, 자극 기법·모드, 밴드 수
static bool tdc_ble_cmd_0x66_sub01_idx01_parameters(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  value;

    live->stimulVolume                   = Rx_dataPacket[index++];  // index++ : 3 → 4
    live->audioVolume                    = Rx_dataPacket[index++];  // index++ : 4 → 5
    live->stimulationIndicatorChannelNum = Rx_dataPacket[index++];  // index++ : 5 → 6

    value                                  = Rx_dataPacket[index++] << 8;     // index++ : 6 → 7
    value                                  = value | Rx_dataPacket[index++];  // index++ : 7 → 8
    live->stimulationIndicatorAmplitude_uA = value;

    live->stimulationStrategy        = Rx_dataPacket[index++];  // index++ : 8 → 9
    live->stimulationMode            = Rx_dataPacket[index++];  // index++ : 9 → 10
    live->firstPulsePhase            = Rx_dataPacket[index++];  // index++ : 10 → 11
    live->stimulationPulsePhaseWidth = Rx_dataPacket[index++];  // index++ : 11 → 12
    live->numFrequencyBand           = Rx_dataPacket[index++];  // index++ : 12 → 13

    // 자극 볼륨 (live->stimulVolume : 1~4)
    if ((live->stimulVolume < 1)      // 1 미만
        || (4 < live->stimulVolume))  // 4 초과 시 에러
    {
        dataRangeError = true;
    }

    // 오디오 볼륨 (live->audioVolume : 1~10)
    if ((live->audioVolume < 1)       // 1 미만
        || (10 < live->audioVolume))  // 10 초과 시 에러
    {
        dataRangeError = true;
    }

    // 알림용 자극 채널 번호 (live->stimulationIndicatorChannelNum : 1~32)
    if ((live->stimulationIndicatorChannelNum < 1)       // 1 미만
        || (32 < live->stimulationIndicatorChannelNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 알림용 자극 크기 uA (live->stimulationIndicatorAmplitude_uA : 1~1800)
    if ((live->stimulationIndicatorAmplitude_uA < 0 /*1*/)   // 0 미만
        || (1800 < live->stimulationIndicatorAmplitude_uA))  // 1800 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 기법 (live->stimulationStrategy : 1~3)
    if ((live->stimulationStrategy < 1)      // 1 미만
        || (3 < live->stimulationStrategy))  // 3 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 모드 (live->stimulationMode : 1~6)
    if ((live->stimulationMode < 1)      // 1 미만
        || (6 < live->stimulationMode))  // 6 초과 시 에러
    {
        dataRangeError = true;
    }

    // 선행 펄스 위상 (live->firstPulsePhase : 0~1)
    if ((live->firstPulsePhase < 0)      // 0 미만
        || (1 < live->firstPulsePhase))  // 1 초과 시 에러
    {
        dataRangeError = true;
    }

    // 펄스 위상 폭 (live->stimulationPulsePhaseWidth : 13~255)
    if ((live->stimulationPulsePhaseWidth < 13)       // 13 미만
        || (255 < live->stimulationPulsePhaseWidth))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 주파수 밴드 (live->numFrequencyBand : 1~32)
    if ((live->numFrequencyBand < 1)       // 1 미만
        || (32 < live->numFrequencyBand))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    return dataRangeError;
}

// 데이터 인덱스 2 - 사용 가능한 자극 전극 번호 [0..16]
static bool tdc_ble_cmd_0x66_sub01_idx02_stim_electrode_0_16(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 0; i < 17; i++)
    {
        live->usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

        if ((live->usableStimulationElectrodIndex[i] < 1)        // 1 미만
            || (100 < live->usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 3 - 사용 가능한 자극 전극 번호 [17..31]
static bool tdc_ble_cmd_0x66_sub01_idx03_stim_electrode_17_31(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 17; i < 32; i++)
    {
        live->usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

        if ((live->usableStimulationElectrodIndex[i] < 1)        // 1 미만
            || (100 < live->usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 4 - 기준 전극 번호 [0..16]
static bool tdc_ble_cmd_0x66_sub01_idx04_ref_electrode_0_16(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 0; i < 17; i++)
    {
        live->usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

        if ((live->usableReferenceElectrodIndex[i] < 1)        // 1 미만
            || (100 < live->usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 5 - 기준 전극 번호 [17..31]
static bool tdc_ble_cmd_0x66_sub01_idx05_ref_electrode_17_31(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 17; i < 32; i++)
    {
        live->usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

        if ((live->usableReferenceElectrodIndex[i] < 1)        // 1 미만
            || (100 < live->usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 6 - 주파수 밴드 출력 순서 [0..16]
static bool tdc_ble_cmd_0x66_sub01_idx06_band_order_0_16(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 0; i < 17; i++)
    {
        live->CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

        if ((live->CIS_FreqBandOrder[i] < 1)        // 1 미만
            || (100 < live->CIS_FreqBandOrder[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 7 - 주파수 밴드 출력 순서 [17..31]
static bool tdc_ble_cmd_0x66_sub01_idx07_band_order_17_31(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;

    for (i = 17; i < 32; i++)
    {
        live->CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

        if ((live->CIS_FreqBandOrder[i] < 1)        // 1 미만
            || (100 < live->CIS_FreqBandOrder[i]))  // 100 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 8 - T 레벨 uA [0..7]
static bool tdc_ble_cmd_0x66_sub01_idx08_t_level_0_7(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 0; i < 8; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->T_level_uA[i] = value;

        if ((live->T_level_uA[i] < 0)         // 0 미만
            || (1800 < live->T_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 9 - T 레벨 uA [8..15]
static bool tdc_ble_cmd_0x66_sub01_idx09_t_level_8_15(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 8; i < 16; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->T_level_uA[i] = value;

        if ((live->T_level_uA[i] < 0)         // 0 미만
            || (1800 < live->T_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 10 - T 레벨 uA [16..23]
static bool tdc_ble_cmd_0x66_sub01_idx10_t_level_16_23(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 16; i < 24; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->T_level_uA[i] = value;

        if ((live->T_level_uA[i] < 0)         // 0 미만
            || (1800 < live->T_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 11 - T 레벨 uA [24..31]
static bool tdc_ble_cmd_0x66_sub01_idx11_t_level_24_31(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 24; i < 32; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->T_level_uA[i] = value;

        if ((live->T_level_uA[i] < 0)         // 0 미만
            || (1800 < live->T_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 12 - C 레벨 uA [0..7]
static bool tdc_ble_cmd_0x66_sub01_idx12_c_level_0_7(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 0; i < 8; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->C_level_uA[i] = value;

        if ((live->C_level_uA[i] < 0)         // 0 미만
            || (1800 < live->C_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 13 - C 레벨 uA [8..15]
static bool tdc_ble_cmd_0x66_sub01_idx13_c_level_8_15(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 8; i < 16; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->C_level_uA[i] = value;

        if ((live->C_level_uA[i] < 0)         // 0 미만
            || (1800 < live->C_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 14 - C 레벨 uA [16..23]
static bool tdc_ble_cmd_0x66_sub01_idx14_c_level_16_23(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 16; i < 24; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->C_level_uA[i] = value;

        if ((live->C_level_uA[i] < 0)         // 0 미만
            || (1800 < live->C_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// 데이터 인덱스 15 - C 레벨 uA [24..31]
static bool tdc_ble_cmd_0x66_sub01_idx15_c_level_24_31(ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live, const uint8_t *Rx_dataPacket, int index)
{
    bool dataRangeError = false;
    int  i;
    int  value;

    for (i = 24; i < 32; i++)
    {
        value              = Rx_dataPacket[index++] << 8;
        value              = value | Rx_dataPacket[index++];
        live->C_level_uA[i] = value;

        if ((live->C_level_uA[i] < 0)         // 0 미만
            || (1800 < live->C_level_uA[i]))  // 1800 초과 시 에러
        {
            dataRangeError = true;
        }
    }

    return dataRangeError;
}

// ===========================================================================
// 하위 명령 1~8
//
// 각 함수 머리에 수락 조건을 한 줄로 적는다. 조건이 없는 명령도 "없음"이라고
// 적어 둔다 - 침묵하면 조건이 없는 것인지 가드를 빠뜨린 것인지 구분되지 않는다.
// ===========================================================================

// 라이브 자극 유지 중(en__HoldOn)에만 수락되는 명령인지 확인한다.
//
// 하위 명령 3(자극 볼륨) · 4(마이크 감도) · 5(알림 자극) · 6(이퀄라이저 읽기)가
// 이 전제를 공유한다. 거부할 때는 앱에 명령 순서 에러를 보내고 서브커맨드를
// Standby 로 되돌린다.
//
// lineNumber 를 인자로 받는 이유 : 여기서 __LINE__ 을 쓰면 네 명령이 전부 같은
// 값을 보내 로그에서 어느 명령이 거부됐는지 구분되지 않는다. 호출 지점 값을
// 그대로 실어 보낸다.
//
// 반환 : 수락 가능하면 true
static bool tdc_ble_cmd_0x66_require_hold_on(ST__MAPPING_PACKET *p_mappingPacket, int lineNumber)
{
    if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
    {
        return true;
    }

    // 라이브 자극 중에만 컨트롤 되는 명령어
    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, lineNumber);
    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;

    return false;
}

// 하위 명령 1 - 전체 파라미터 전달 (en__allParameter)
//
// 데이터 인덱스 1~15 를 순차로 받는다. 아래 switch 가 이 하위 명령의
// 목차다 - 어느 인덱스가 무엇을 담는지는 각 함수 이름과 주석에 있다.
// 수락 조건 : 없음. 다만 데이터 인덱스는 1씩 증가해야 한다.
static void tdc_ble_cmd_0x66_sub01_all_param(const uint8_t *Rx_dataPacket, int index)
{
    ST__MAPPING_PACKET                  *p_mappingPacket = tdc_ble_mapping_get_packet();
    ST__MAPPINGPAYLOAD_LIVE_STIMULATION *live            = &p_mappingPacket->tdc_isd_map_live_step;

    int  i;
    int  subCommandData_Num_index;
    bool dataRangeError = false;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    live->subCommand = en__Standby;

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

        // 데이터 인덱스 1~15.
        //
        // default 는 죽은 코드가 아니다. 데이터 인덱스 카운터를 map_flash 와
        // 공유하는데 flash 는 인덱스를 34 까지 쓰므로, flash 가 카운터를
        // 올려 둔 상태에서 이 명령이 오면 16 이상이 실제로 들어온다.
        // clang-format off
        switch (subCommandData_Num_index)
        {
            case  1: dataRangeError = tdc_ble_cmd_0x66_sub01_idx01_parameters          (live, Rx_dataPacket, index); break;
            case  2: dataRangeError = tdc_ble_cmd_0x66_sub01_idx02_stim_electrode_0_16 (live, Rx_dataPacket, index); break;
            case  3: dataRangeError = tdc_ble_cmd_0x66_sub01_idx03_stim_electrode_17_31(live, Rx_dataPacket, index); break;
            case  4: dataRangeError = tdc_ble_cmd_0x66_sub01_idx04_ref_electrode_0_16  (live, Rx_dataPacket, index); break;
            case  5: dataRangeError = tdc_ble_cmd_0x66_sub01_idx05_ref_electrode_17_31 (live, Rx_dataPacket, index); break;
            case  6: dataRangeError = tdc_ble_cmd_0x66_sub01_idx06_band_order_0_16     (live, Rx_dataPacket, index); break;
            case  7: dataRangeError = tdc_ble_cmd_0x66_sub01_idx07_band_order_17_31    (live, Rx_dataPacket, index); break;
            case  8: dataRangeError = tdc_ble_cmd_0x66_sub01_idx08_t_level_0_7         (live, Rx_dataPacket, index); break;
            case  9: dataRangeError = tdc_ble_cmd_0x66_sub01_idx09_t_level_8_15        (live, Rx_dataPacket, index); break;
            case 10: dataRangeError = tdc_ble_cmd_0x66_sub01_idx10_t_level_16_23       (live, Rx_dataPacket, index); break;
            case 11: dataRangeError = tdc_ble_cmd_0x66_sub01_idx11_t_level_24_31       (live, Rx_dataPacket, index); break;
            case 12: dataRangeError = tdc_ble_cmd_0x66_sub01_idx12_c_level_0_7         (live, Rx_dataPacket, index); break;
            case 13: dataRangeError = tdc_ble_cmd_0x66_sub01_idx13_c_level_8_15        (live, Rx_dataPacket, index); break;
            case 14: dataRangeError = tdc_ble_cmd_0x66_sub01_idx14_c_level_16_23       (live, Rx_dataPacket, index); break;
            case 15: dataRangeError = tdc_ble_cmd_0x66_sub01_idx15_c_level_24_31       (live, Rx_dataPacket, index); break;

            default: dataRangeError = true;                                                                          break;
        }
        // clang-format on

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
                    live->audio_input_x_mim[i] = df_minAudioForLogarithm;
                }

                // x- max
                for (i = 0; i < 32; i++)
                {
                    live->audio_input_x_max[i] = df_maxAudioForLogarithm;
                }

                live->subCommand = en__allParameter;

                TDC_PRINTF_I("[LIVE] LIVE ALL PARAMETER PAYLOAD NUM : NOW, SUB COMMAND SET TO EN__ALL_PARAMETER \r\n");
            }
            else
            {
                live->subCommand = en__Standby;
            }
        }
    }
}

// 하위 명령 2 - 라이브 모드 시작 (en__Start)
// 수락 조건 : 없음. 어느 상태에서도 수락한다.
static void tdc_ble_cmd_0x66_sub02_start(void)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Start;
}

// 하위 명령 3 - 자극 볼륨 조절 (en__StimulationVolumeAdjust)
// 수락 조건 : 라이브 자극 유지 중(en__HoldOn)에만.
static void tdc_ble_cmd_0x66_sub03_volume_adjust(const uint8_t *Rx_dataPacket, int index)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int value;

    if (!tdc_ble_cmd_0x66_require_hold_on(p_mappingPacket, __LINE__))
    {
        return;
    }

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

// 하위 명령 4 - 마이크 감도 조절 (en__MicSensitivityAdjust)
// 수락 조건 : 라이브 자극 유지 중(en__HoldOn)에만.
static void tdc_ble_cmd_0x66_sub04_mic_sensitivity(const uint8_t *Rx_dataPacket, int index)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int value;

    if (!tdc_ble_cmd_0x66_require_hold_on(p_mappingPacket, __LINE__))
    {
        return;
    }

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

// 하위 명령 5 - 알림 자극 출력 (en__mapping_Stimul_indicator)
// 수락 조건 : 라이브 자극 유지 중(en__HoldOn)에만.
static void tdc_ble_cmd_0x66_sub05_indicator(const uint8_t *Rx_dataPacket, int index)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int i;
    int value;

    // 알림 자극의 자극 크기 상·하한. 맵 데이터를 훑어 갱신하며 호출 간에
    // 값을 유지해야 하므로 static 이다(원본 fetch_packet 과 동일).
    static int max_C_uA = 0, min_T_uA = 1800;

    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;

    if (!tdc_ble_cmd_0x66_require_hold_on(p_mappingPacket, __LINE__))
    {
        return;
    }

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

// 하위 명령 6 - 자극 출력 값 읽기 · 이퀄라이저 (en__readEqualizer)
// 수락 조건 : 라이브 자극 유지 중(en__HoldOn)에만.
static void tdc_ble_cmd_0x66_sub06_read_equalizer(const uint8_t *Rx_dataPacket, int index)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  tempA, tempB;
    bool dataRangeError = false;

    if (!tdc_ble_cmd_0x66_require_hold_on(p_mappingPacket, __LINE__))
    {
        return;
    }

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

// 하위 명령 7 - 장치 상태 읽기 (en__readDeviceStatus)
// 수락 조건 : 없음. 어느 상태에서도 수락한다.
static void tdc_ble_cmd_0x66_sub07_read_device_status(void)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__readDeviceStatus;
}

// 하위 명령 8 - 라이브 모드 종료 (en__Stop)
// 수락 조건 : 없음. 다만 fetch_packet 의 재진입 가드에서 먼저 걸릴 수 있다.
static void tdc_ble_cmd_0x66_sub08_stop(void)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Stop;
}

// ===========================================================================
// 명령 0x66 진입점
// ===========================================================================

void tdc_ble_cmd_0x66_live(const uint8_t *Rx_dataPacket)  // 0x66
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
        case en__allParameter:  // 하위 명령 1 (전체 파라미터 전달)
            tdc_ble_cmd_0x66_sub01_all_param(Rx_dataPacket, index);
            break;

        case en__Start:  // 하위 명령 2 (실시간 자극 시작)
            tdc_ble_cmd_0x66_sub02_start();
            break;

        case en__StimulationVolumeAdjust:  // 하위 명령 3 (자극 볼륨 조절)
            tdc_ble_cmd_0x66_sub03_volume_adjust(Rx_dataPacket, index);
            break;

        case en__MicSensitivityAdjust:  // 하위 명령 4 (마이크 감도 조절)
            tdc_ble_cmd_0x66_sub04_mic_sensitivity(Rx_dataPacket, index);
            break;

        case en__mapping_Stimul_indicator:  // 하위 명령 5 (알림 자극 출력)
            tdc_ble_cmd_0x66_sub05_indicator(Rx_dataPacket, index);
            break;

        case en__readEqualizer:  // 하위 명령 6 (자극 출력 값 읽기: 이퀄라이저)
            tdc_ble_cmd_0x66_sub06_read_equalizer(Rx_dataPacket, index);
            break;

        case en__readDeviceStatus:  // 하위 명령 7 (장치 상태 읽기)
            tdc_ble_cmd_0x66_sub07_read_device_status();
            break;

        case en__Stop:  // 하위 명령 8 (실시간 자극 종료)
            tdc_ble_cmd_0x66_sub08_stop();
            break;

        // 하위 9 (en__HoldOn) - 수락 조건 : 앱이 보낼 수 없는 내부 상태값이다.
        // 위 범위 검사(1~9)는 통과하지만 대응 case 가 없어 여기로 낙하하고,
        // 서브커맨드를 Standby 로 되돌린다.
        default:
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            break;
    }
}
