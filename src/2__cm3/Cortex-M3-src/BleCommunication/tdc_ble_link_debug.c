#include <tdc_ble_link_debug.h>
#include <tdc_link_config.h>
#include <tdc_link_control.h>
#include <cfx_cm3_sharedMemory.h>

#define TDC_LINK_OPTION 0x20
#define TDC_LINK_VERSION_OPTION 0xFF
#define TDC_LINK_VERSION_MAJOR 1
#define TDC_LINK_VERSION_MINOR 0

static int reject(int command, uint8_t *tx, int result, unsigned line)
{
    tx[0] = 0xF0;
    tx[1] = (uint8_t)command;
    tx[2] = (uint8_t)(result == TDC_LINK_CONFIG_IO_ERROR ? 10 : 8);
    tx[3] = (uint8_t)(result == TDC_LINK_CONFIG_IO_ERROR ? 2 : 6);
    tx[4] = (uint8_t)(line >> 8);
    tx[5] = (uint8_t)line;
    return 6;
}

static int read_value(unsigned index)
{
    const tdc_link_config_t *config = tdc_link_config_get();
    switch (index)
    {
        case 1: return tdc_link_control_get_level();
        case 2: return (int)config->period_100ms;
        case 3: return (int)config->min_level;
        case 4: return (int)config->mapping_min_level;
        case 5: return (int)config->step_up;
        case 6: return tdc_link_control_get_force();
        case 7: return tdc_link_control_get_infinite() ? 255 : (int)config->coin_count;
        case 8: return tdc_link_control_get_infinite();
        case 9: return (int)config->mode;
        case 10: return (int)config->accel;
        case 11: return tdc_link_control_get_nop();
        /* 전체15값 묶음은 원본처럼 각 값의 하위8비트다.
         * 개별 펄스폭 조회는2바이트로 보내255us 초과도 손실 없이 알린다. */
        case 12: return (int)getPointerCurrentMapData()->stimulationPulsePhaseWidth;
        case 13: return getPointerCalculatedStimulPara_byCM3()->frameNumPerChannel;
        case 14: return tdc_link_control_get_override();
        case 15: return (int)getPointerCurrentMapData()->stimulationStrategy;
        case 16: return (int)config->max_level;
        case 17: return 0; /* FF는 구현하지 않는다. */
        default: return -1;
    }
}

static int write_value(unsigned index, unsigned value)
{
    int result;
    switch (index)
    {
        case 2: case 3: case 4: case 5: case 9: case 10: case 16:
            return tdc_link_config_set(index, value);
        case 6:
            return tdc_link_control_set_force((int)value) ? 0 : TDC_LINK_CONFIG_RANGE_ERROR;
        case 7:
            if (value == 255)
            {
                tdc_link_control_set_infinite(1);
                return 0;
            }
            result = tdc_link_config_set(index, value);
            if (result == 0) tdc_link_control_set_infinite(0);
            return result;
        case 8:
            if (value > 1) return TDC_LINK_CONFIG_RANGE_ERROR;
            tdc_link_control_set_infinite((int)value);
            return 0;
        case 11:
            return tdc_link_control_set_nop((int)value) ? 0 : TDC_LINK_CONFIG_RANGE_ERROR;
        case 14:
            if (value > 1) return TDC_LINK_CONFIG_RANGE_ERROR;
            tdc_link_control_set_override((int)value);
            return 0;
        case 17:
            return value == 0 ? 0 : TDC_LINK_CONFIG_RANGE_ERROR;
        default:
            return TDC_LINK_CONFIG_RANGE_ERROR;
    }
}

int tdc_ble_link_debug_handle(int command, const int *data, uint8_t *tx)
{
    unsigned option = (unsigned)data[0];
    unsigned access = (unsigned)data[1];
    unsigned index = (unsigned)data[2];
    int result;
    int length;
    if (access > 1 || index > 255) return reject(command, tx, TDC_LINK_CONFIG_RANGE_ERROR, __LINE__);
    if (option == TDC_LINK_VERSION_OPTION)
    {
        static const uint8_t bitmap[8] = {0x1E, 0, 0, 0, 0x01, 0, 0, 0};
        if (access != 0 || index != 0) return reject(command, tx, TDC_LINK_CONFIG_RANGE_ERROR, __LINE__);
        tx[0] = (uint8_t)command;
        tx[1] = TDC_LINK_VERSION_OPTION;
        tx[2] = 0;
        tx[3] = 0;
        tx[4] = 10;
        tx[5] = TDC_LINK_VERSION_MAJOR;
        tx[6] = TDC_LINK_VERSION_MINOR;
        for (unsigned i = 0; i < 8; i++) tx[7 + i] = bitmap[i];
        return 15;
    }
    if (option != TDC_LINK_OPTION || index > 17)
        return reject(command, tx, TDC_LINK_CONFIG_RANGE_ERROR, __LINE__);
    if (access == 1)
    {
        if ((unsigned)data[3] > 255) return reject(command, tx, TDC_LINK_CONFIG_RANGE_ERROR, __LINE__);
        result = write_value(index, (unsigned)data[3]);
        if (result != TDC_LINK_CONFIG_OK) return reject(command, tx, result, __LINE__);
    }
    length = index == 0 ? 15 : (index == 12 ? 2 : 1);
    tx[0] = (uint8_t)command;
    tx[1] = TDC_LINK_OPTION;
    tx[2] = (uint8_t)access;
    tx[3] = (uint8_t)index;
    tx[4] = (uint8_t)length;
    if (index == 12)
    {
        result = read_value(index);
        tx[5] = (uint8_t)(result >> 8);
        tx[6] = (uint8_t)result;
        return 7;
    }
    for (int i = 0; i < length; i++)
    {
        result = read_value(index == 0 ? (unsigned)(i + 1) : index);
        if (result < 0) return reject(command, tx, TDC_LINK_CONFIG_RANGE_ERROR, __LINE__);
        tx[5 + i] = (uint8_t)result;
    }
    return 5 + length;
}
