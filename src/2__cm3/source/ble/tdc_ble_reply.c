
#include <stdint.h>
#include <tdc_ble_reply.h>

int tdc_ble_reply_header(uint8_t *tx_buf, int tx_index, int command)
{
    return tdc_ble_reply_u8(tx_buf, tx_index, command);
}

int tdc_ble_reply_u8(uint8_t *tx_buf, int tx_index, int value)
{
    tx_buf[tx_index++] = value;

    return tx_index;
}

int tdc_ble_reply_u16(uint8_t *tx_buf, int tx_index, int value)
{
    // 대상이 uint8_t 라 대입 시 하위 8비트만 남는다.
    // 기존 코드의 표기 변형(마스크 생략 · (char) 캐스팅 · 0x00FF 표기)은
    // 전부 이 구현과 결과가 같다. 값이 음수여도 동일하다.
    tx_buf[tx_index++] = (value >> 8) & 0xFF;  // 상위 바이트
    tx_buf[tx_index++] = value & 0xFF;         // 하위 바이트

    return tx_index;
}

int tdc_ble_reply_u32(uint8_t *tx_buf, int tx_index, int value)
{
    tx_buf[tx_index++] = (value >> 24) & 0xFF;
    tx_buf[tx_index++] = (value >> 16) & 0xFF;
    tx_buf[tx_index++] = (value >> 8) & 0xFF;
    tx_buf[tx_index++] = value & 0xFF;

    return tx_index;
}
