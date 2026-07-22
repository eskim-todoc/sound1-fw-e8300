
#include <tdc_crc.h>

uint16_t tdc_crc_ccitt_calc(void* buf, uint32_t size)
{
    uint32_t i;
    uint8_t* p = (uint8_t*) buf;

    Sys_Set_CRC_Config(CRC, CRC_LITTLE_ENDIAN | CRC_CCITT | CRC_BIT_ORDER_STANDARD | CRC_FINAL_XOR_STANDARD);

    Sys_CRC_CCITTInitValue(CRC);

    // 4바이트 벌크 처리
    while (size >= 4)
    {
        // 리틀엔디언 하드웨어가 내부적으로 LSB->MSB 순으로 처리
        uint32_t w = ((uint32_t) p[0])            //
                     | (((uint32_t) p[1]) << 8)   //
                     | (((uint32_t) p[2]) << 16)  //
                     | (((uint32_t) p[3]) << 24);

        Sys_CRC_Add(CRC, w, 32);

        p += 4;
        size -= 4;
    }

    // 나머지 1 ~ 3 바이트: 메모리 경계 안전하게 바이트 단위 입력
    while (size--)
    {
        Sys_CRC_Add(CRC, *p++, 8);
    }

    // CCITT는 16비트가 유효
    return (uint16_t) Sys_CRC_GetFinalValue(CRC);
}
