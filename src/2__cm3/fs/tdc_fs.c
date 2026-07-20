/**
 * @file tdc_fs.c
 */

#include <tdc_fs.h>

#include <ff.h>

TDC_FS_FFT_PASS_BIN_T *g_tdc_fs_ptr_pass_bin;
TDC_FS_ENTIRE_MAP_T   *g_tdc_fs_ptr_entire_map;

FATFS g_tdc_fs_mount;
FIL   g_tdc_fs_ohdl;

FIL *ci_fatfs_get_fp(void)
{
    return &g_tdc_fs_ohdl;
}

int tdc_fs_nvm_init(void)
{
    if (NVMIsInit())
    {
        if (NVMReInitOptions(NULL) == ARM_DRIVER_OK)
        {
            return df_True;
        }
    }
    else
    {
        if (NVMInit(NULL) == ARM_DRIVER_OK)
        {
            return df_True;
        }
    }

    return df_False;
}

int tdc_fs_fatfs_init_mem_map(void)
{
    g_tdc_fs_ptr_pass_bin   = (TDC_FS_FFT_PASS_BIN_T *) TDC_FS_BASE_ADDR_FFT_PASS_BIN;
    g_tdc_fs_ptr_entire_map = (TDC_FS_ENTIRE_MAP_T *) TDC_FS_BASE_ADDR_ENTIRE_MAP;

    return df_True;
}

int tdc_fs_fatfs_remount(int ldrv)
{
    if (tdc_fs_fatfs_unmount())
    {
        return tdc_fs_fatfs_mount(ldrv);
    }

    return df_False;
}

int tdc_fs_fatfs_mount(int ldrv)
{
    DIR         dir;
    FRESULT     fr;
    const char *path;

    if (ldrv == 0)
    {
        path = "0:";
    }
    else
    {
        path = "1:";
    }

    // option 1: force mount
    if (f_mount(&g_tdc_fs_mount, path, /* option */ 1) != FR_OK)
    {
        TDC_PRINTF_E("[FATFS] FAIL : MOUNT '%s' \r\n", path);
        return df_False;
    }

    if (f_chdrive(path) != FR_OK)
    {
        TDC_PRINTF_E("[FATFS] FAIL : CHANGE DRIVE '%s' \r\n", path);
        return df_False;
    }

    if (f_opendir(&dir, path) != FR_OK)
    {
        TDC_PRINTF_E("[FATFS] FAIL : OPEN DIR '%s' \r\n", path);
        return df_False;
    }

    if (f_closedir(&dir) != FR_OK)
    {
        TDC_PRINTF_E("[FATFS] FAIL : CLOSE DIR '%s' \r\n", path);
        return df_False;
    }

    return df_True;
}

int tdc_fs_fatfs_unmount(void)
{
    FRESULT     fr;
    const char *path;

    // 현재 마운트 된 드라이브를 해제한다.

    // 마운트 된 상태 아니면 즉시 True로 종료 (0: not mounted)
    if (g_tdc_fs_mount.fs_type == 0)
    {
        return df_True;
    }

    // Logical 드라이브 번호 확인 후, 드라이브 unmount, 드라이브 0이 아니면 전부 1로 처리
    if (g_tdc_fs_mount.ldrv == 0)
    {
        path = "0:";
    }
    else
    {
        path = "1:";
    }

    fr = f_unmount(path);

    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[FATFS] UMOUNT FAILED, DRIVE : '%s' \r\n", path);
        return df_False;
    }

    return df_True;
}

int tdc_fs_mount(void)
{
    int ret;

    ret = f_mount(&g_tdc_fs_mount, TDC_FS_LOGICAL_DRIVE_NUM, TDC_FS_MOUNT_OPTION);

    if (ret != FR_OK)
    {
        TDC_PRINTF_E("[FS] FAILED TO MOUNT DRIVE ('%s') \r\n", TDC_FS_LOGICAL_DRIVE_NUM);
        return df_False;
    }

    SYS_WATCHDOG_REFRESH();

    ret = f_chdrive(TDC_FS_LOGICAL_DRIVE_NUM);

    if (ret != FR_OK)
    {
        TDC_PRINTF_E("[FS] FAILED TO CHANGE DRIVE ('%s') \r\n", TDC_FS_LOGICAL_DRIVE_NUM);
        return df_False;
    }

    SYS_WATCHDOG_REFRESH();

    g_tdc_fs_ptr_pass_bin   = (TDC_FS_FFT_PASS_BIN_T *) TDC_FS_BASE_ADDR_FFT_PASS_BIN;
    g_tdc_fs_ptr_entire_map = (TDC_FS_ENTIRE_MAP_T *) TDC_FS_BASE_ADDR_ENTIRE_MAP;

    return df_True;
}

int tdc_fs_read_with_crc_and_aes128(char     *p_name,  //
                                           uint8_t  *p_data,
                                           int       data_size,
                                           uint32_t *p_uint32_crc,
                                           uint32_t *p_uint32_aes128_padding,
                                           int       aes128_padding_size,
                                           bool      enable_crc,
                                           bool      enable_aes)
{
    int      ret = -1;
    FRESULT  fr;
    UINT     br;
    uint8_t *p_uint8_crc            = (uint8_t *) p_uint32_crc;
    uint8_t *p_uint8_aes128_padding = (uint8_t *) p_uint32_aes128_padding;

#if 1
    TDC_PRINTF_D("[FS] READ --> NAME : '%s', DATA ADDR : 0x%08X, SIZE : %d, ", p_name, (uint32_t) p_data, data_size);
    TDC_PRINTF_D("CRC ADDR : 0x%08X, AES128 PADDING ADDR : 0x%08X, ", (uint32_t) p_uint32_crc, (uint32_t) p_uint32_aes128_padding);
    TDC_PRINTF_D("PADDING SIZE : %d, ENABLE CRC : %u, ENABLE AES128 : %u \r\n", aes128_padding_size, enable_crc, enable_aes);
#endif

    // ---- 파일 열기 (읽기 전용) ----
    fr = f_open(&g_tdc_fs_ohdl, p_name, FA_OPEN_EXISTING | FA_READ);
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[FS] OPEN FAIL '%s' (FRESULT=%d)\r\n", p_name, fr);
        return ret;
    }

    fr = f_lseek(&g_tdc_fs_ohdl, 0);
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[FS] LSEEK FAIL '%s' (FRESULT=%d)\r\n", p_name, fr);
        f_close(&g_tdc_fs_ohdl);
        return ret;
    }

    // ---- 파일 읽기 ----
    fr = f_read(&g_tdc_fs_ohdl, p_data, data_size, &br);
    if (fr != FR_OK || (int) br != data_size)
    {
        TDC_PRINTF_E("[FS] READ DATA FAIL '%s' (res=%d, got=%u)\r\n", p_name, fr, br);
        f_close(&g_tdc_fs_ohdl);
        return ret;
    }

    // ---- CRC 기능 사용 시 ----
    if (enable_crc)
    {
        // ---- CRC 암호문 4바이트 읽기 ----
        fr = f_read(&g_tdc_fs_ohdl, p_uint32_crc, 4, &br);
        if (fr != FR_OK || br != 4)
        {
            TDC_PRINTF_E("[FS] READ CRC FAIL '%s' (res=%d, got=%u)\r\n", p_name, fr, br);
            f_close(&g_tdc_fs_ohdl);
            return ret;
        }
    }
    else
    {
        // CRC 사용하지 않을 시 값을 0으로 초기화
        *p_uint32_crc = 0;
    }

    // ---- AES128 사용 시 ----
    if (enable_aes)
    {
        // ---- 패딩 암호문 읽기 (0일 수도 있음) ----
        if (aes128_padding_size > 0)
        {
            fr = f_read(&g_tdc_fs_ohdl, p_uint32_aes128_padding, aes128_padding_size, &br);
            if (fr != FR_OK || (int) br != aes128_padding_size)
            {
                TDC_PRINTF_E("[FS] READ PAD FAIL '%s' (res=%d, got=%u)\r\n", p_name, fr, br);
                f_close(&g_tdc_fs_ohdl);
                return ret;
            }
        }
    }

#if 0
    {
        SEGGER_RTT_printf(0, "\r\n");

        if (enable_aes)
        {
            TDC_PRINTF("[FS] '%s' BEFORE DECRYPTION \r\n\n", p_name);
        }
        else
        {
            TDC_PRINTF("[FS] '%s' BEFORE DECRYPTION (ACTUALLY AES128 NOT USED) \r\n\n", p_name);
        }

        TDC_PRINTF("[FS] DATA \r\n");

        for (int print_i = 0; print_i < data_size; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", p_data[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == data_size))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (enable_crc)
        {
            TDC_PRINTF("[FS] CRC \r\n");
        }
        else
        {
            TDC_PRINTF("[FS] CRC (ACTUALLY CRC NOT USED) \r\n");
        }

        for (int print_i = 0; print_i < 4; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_crc)[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == 4))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (aes128_padding_size > 0)
        {
            TDC_PRINTF("[FS] PADDING \r\n");

            for (int print_i = 0; print_i < aes128_padding_size; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_aes128_padding)[print_i]);

                if (((print_i + 1) % 32 == 0) || ((print_i + 1) == aes128_padding_size))
                {
                    SEGGER_RTT_printf(0, "\r\n");
                }
            }

            SEGGER_RTT_printf(0, "\r\n");
        }
    }
#endif

    if (enable_aes)
    {
        // ---- 복호화 (ECB) ----

        // 1) data의 16바이트 정블록
        int full_blocks = data_size / 16;
        int remain      = data_size % 16;

        // 정블록 복호화 (in-place)
        uint8_t *p = p_data;

        for (int i = 0; i < full_blocks; ++i)
        {
            ci_aes_decrypt(p);  // 16B block
            p += 16;
        }

        // 2) 마지막 16바이트 블록 구성: [data_tail(remain)] + [crc 4B] + [pad N]  => 총 16B 여야 함
        int last_block_tail = remain + 4 + aes128_padding_size;

        if (last_block_tail <= 0 || last_block_tail > 16)
        {
            TDC_PRINTF_E("[FS] INVALID TAIL (%d) remain=%d pad=%d\r\n", last_block_tail, remain, aes128_padding_size);
            f_close(&g_tdc_fs_ohdl);
            return ret;
        }

        uint8_t blk[16] = {0};

        // a) data의 남은 암호문 바이트 복사
        if (remain > 0)
        {
            memcpy(blk, p, remain);
        }

        // b) 이어서 CRC 암호문 4바이트
        memcpy(blk + remain, p_uint8_crc, 4);

        // c) 이어서 패딩 암호문 N바이트
        if (aes128_padding_size > 0)
        {
            memcpy(blk + remain + 4, p_uint8_aes128_padding, aes128_padding_size);
        }

        // d) 마지막 블록 복호화
        ci_aes_decrypt(blk);

        // e) 평문 재배치: data tail, CRC, PAD
        if (remain > 0)
        {
            memcpy(p, blk, remain);
        }

        memcpy(p_uint8_crc, blk + remain, 4);

        if (aes128_padding_size > 0)
        {
            memcpy(p_uint8_aes128_padding, blk + remain + 4, aes128_padding_size);
        }
    }

    // ---- CRC 검증 ----
    if (enable_crc)
    {
        // ---- CRC 체크 (하위 16비트만 유효) ----
        uint16_t crc_calc = tdc_crc_ccitt_calc(p_data, data_size);
        uint16_t crc_file = (uint16_t) (*p_uint32_crc & 0xFFFF);

        TDC_PRINTF_V("[FS] '%s' CRC FILE=%u CALC=%u\r\n", p_name, (unsigned) crc_file, (unsigned) crc_calc);

        f_close(&g_tdc_fs_ohdl);

#if 0
        {
            SEGGER_RTT_printf(0, "\r\n");

            if (enable_aes)
            {
                TDC_PRINTF("[FS] '%s' AFTER DECRYPTION \r\n\n", p_name);
            }
            else
            {
                TDC_PRINTF("[FS] '%s' AFTER DECRYPTION (ACTUALLY AES128 NOT USED) \r\n\n", p_name);
            }

            TDC_PRINTF("[FS] DATA \r\n");

            for (int print_i = 0; print_i < data_size; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", p_data[print_i]);

                if (((print_i + 1) % 32 == 0) || ((print_i + 1) == data_size))
                {
                    SEGGER_RTT_printf(0, "\r\n");
                }
            }

            SEGGER_RTT_printf(0, "\r\n");

            if (enable_crc)
            {
                TDC_PRINTF("[FS] CRC \r\n");
            }
            else
            {
                TDC_PRINTF("[FS] CRC (ACTUALLY CRC NOT USED) \r\n");
            }

            for (int print_i = 0; print_i < 4; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_crc)[print_i]);

                if (((print_i + 1) % 32 == 0) || ((print_i + 1) == 4))
                {
                    SEGGER_RTT_printf(0, "\r\n");
                }
            }

            SEGGER_RTT_printf(0, "\r\n");

            if (aes128_padding_size > 0)
            {
                TDC_PRINTF("[FS] PADDING \r\n");

                for (int print_i = 0; print_i < aes128_padding_size; print_i++)
                {
                    SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_aes128_padding)[print_i]);

                    if (((print_i + 1) % 32 == 0) || ((print_i + 1) == aes128_padding_size))
                    {
                        SEGGER_RTT_printf(0, "\r\n");
                    }
                }

                SEGGER_RTT_printf(0, "\r\n");
            }
        }
#endif

        ret = (crc_file == crc_calc) ? 0 : -1;
    }
    else
    {
        ret = 0;
    }

    return ret;
}

int tdc_fs_write_with_crc_and_aes128(char     *p_name,
                                            uint8_t  *p_data,                   // 평문 데이터(data_size)
                                            int       data_size,                // 예: 132
                                            uint32_t *p_uint32_crc,             // 4B (하위 16비트만 유효)
                                            uint32_t *p_uint32_aes128_padding,  // 패딩 버퍼(쓰기 전용, 0 채움 권장)
                                            int       aes128_padding_size,      // 예: 8  (remain+4+pad==16 충족)
                                            bool      enable_crc,
                                            bool      enable_aes)
{
    int ret = -1;

#if 1
    TDC_PRINTF_D("[FS] WRITE --> NAME : '%s', DATA ADDR : 0x%08X, SIZE : %d, ", p_name, (uint32_t) p_data, data_size);
    TDC_PRINTF_D("CRC ADDR : 0x%08X, AES128 PADDING ADDR : 0x%08X, ", (uint32_t) p_uint32_crc, (uint32_t) p_uint32_aes128_padding);
    TDC_PRINTF_D("PADDING SIZE : %d, ENABLE CEC : %u, ENABLE AES128 : %u \r\n", aes128_padding_size, enable_crc, enable_aes);
#endif

    if (data_size < 0)
    {
        return ret;
    }

    if (aes128_padding_size < 0)
    {
        return ret;
    }

    // 마지막 블록 불변식 검증
    int remain = data_size % 16;
    if ((remain + 4 + aes128_padding_size) != 16)
    {
        TDC_PRINTF_E("[FS] INVALID TAIL: REMAIN(%d) + 4 + PAD(%d) != 16\r\n", remain, aes128_padding_size);
        return ret;
    }

    if (enable_crc)
    {
        // CRC16 계산 (평문 data 전체 대상)
        uint16_t crc16 = tdc_crc_ccitt_calc(p_data, data_size);
        // 하위 16비트에 기록, 상위 16비트는 0
        *p_uint32_crc = (uint32_t) crc16;
    }
    else
    {
        *p_uint32_crc = 0;
    }

    TDC_PRINTF_I("[FS] CRC CALC : %u \r\n", *p_uint32_crc);

    // 패딩 바이트는 0 채움 권장(고정값) : 나중에 용도 생기면 활용
    if (aes128_padding_size > 0 && p_uint32_aes128_padding)
    {
        memset(p_uint32_aes128_padding, 0, (size_t) aes128_padding_size);
    }

#if 0
    {
        SEGGER_RTT_printf(0, "\r\n");

        if (enable_aes)
        {
            TDC_PRINTF("[FS] '%s' BEFORE ENCRYPTION \r\n\n", p_name);
        }
        else
        {
            TDC_PRINTF("[FS] '%s' BEFORE ENCRYPTION (ACTUALLY AES128 NOT USED) \r\n\n", p_name);
        }

        TDC_PRINTF("[FS] DATA \r\n");

        for (int print_i = 0; print_i < data_size; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", p_data[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == data_size))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (enable_crc)
        {
            TDC_PRINTF("[FS] CRC \r\n");
        }
        else
        {
            TDC_PRINTF("[FS] CRC (ACTUALLY CRC NOT USED) \r\n");
        }

        for (int print_i = 0; print_i < 4; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", ((uint8_t *) p_uint32_crc)[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == 4))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (aes128_padding_size > 0)
        {
            TDC_PRINTF("[FS] PADDING \r\n");

            for (int print_i = 0; print_i < aes128_padding_size; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", ((uint8_t *) p_uint32_aes128_padding)[print_i]);

                if (((print_i + 1) % 32 == 0) || ((print_i + 1) == aes128_padding_size))
                {
                    SEGGER_RTT_printf(0, "\r\n");
                }
            }

            SEGGER_RTT_printf(0, "\r\n");
        }
    }
#endif

    // 파일 열기(덮어쓰기)
    // FRESULT fr = f_open(&g_tdc_fs_ohdl, p_name, FA_CREATE_ALWAYS | FA_WRITE);
    FRESULT fr = f_open(&g_tdc_fs_ohdl, p_name, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[FS] OPEN FAIL '%s' (FRESULT=%d)\r\n", p_name, fr);
        return ret;
    }

    f_lseek(&g_tdc_fs_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_tdc_fs_ohdl, 0);
    TDC_PRINTF_V("[FS] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", p_name);

    UINT bw = 0;

    // 1) data의 16바이트 정블록을 암호화하여 그대로 기록
    int            full_blocks = data_size / 16;
    const uint8_t *p           = p_data;

    for (int i = 0; i < full_blocks; ++i)
    {
        uint8_t blk[16];
        memcpy(blk, p, 16);

        if (enable_aes)
        {
            ci_aes_encrypt(blk);  // 16B in-place 암호화

#if 0
            SEGGER_RTT_printf(0, "BLK_[%4d] : ", i);

            for (int print_i = 0; print_i < 16; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", blk[print_i]);
            }

            SEGGER_RTT_printf(0, "\r\n");
#endif
        }

        fr = f_write(&g_tdc_fs_ohdl, blk, 16, &bw);
        if (fr != FR_OK || bw != 16)
        {
            TDC_PRINTF_E("[FS] WRITE DATA BLOCK FAIL '%s' (res=%d, wrote=%u)\r\n", p_name, fr, bw);
            f_close(&g_tdc_fs_ohdl);
            return ret;
        }

        p += 16;
    }

    // 2) 마지막 16바이트 블록 구성: [data_tail(remain)] + [CRC 4B] + [padding N] → 암호화
    {
        uint8_t last_plain[16] = {0};
        uint8_t last_cipher[16];

        if (remain > 0)
        {
            memcpy(last_plain, p, (size_t) remain);
        }

        // CRC 4바이트 (LE 기준으로 메모리에 들어있다고 가정)
        memcpy(last_plain + remain, (uint8_t *) p_uint32_crc, 4);

        // 패딩 N바이트
        if ((aes128_padding_size > 0) && p_uint32_aes128_padding)
        {
            memcpy(last_plain + remain + 4, (uint8_t *) p_uint32_aes128_padding, (size_t) aes128_padding_size);
        }

        // 암호화
        memcpy(last_cipher, last_plain, 16);

        if (enable_aes)
        {
            ci_aes_encrypt(last_cipher);
#if 0
            SEGGER_RTT_printf(0, "BLK_[%4d] : ", full_blocks);

            for (int print_i = 0; print_i < 16; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", last_cipher[print_i]);
            }

            SEGGER_RTT_printf(0, "\r\n");
#endif
        }

        // 파일에는 read 로직과 대칭되게 "분할"하여 기록:
        //  - 먼저 data의 남은 부분(remain) 만큼을 data영역의 연속으로 기록
        if (remain > 0)
        {
            fr = f_write(&g_tdc_fs_ohdl, last_cipher, (UINT) remain, &bw);
            if (fr != FR_OK || (int) bw != remain)
            {
                TDC_PRINTF_E("[FS] WRITE DATA TAIL FAIL '%s' (res=%d, wrote=%u)\r\n", p_name, fr, bw);
                f_close(&g_tdc_fs_ohdl);
                return ret;
            }
        }

        //  - 다음 4바이트는 CRC 영역에 해당
        fr = f_write(&g_tdc_fs_ohdl, last_cipher + remain, 4, &bw);
        if (fr != FR_OK || bw != 4)
        {
            TDC_PRINTF_E("[FS] WRITE CRC FAIL '%s' (res=%d, wrote=%u)\r\n", p_name, fr, bw);
            f_close(&g_tdc_fs_ohdl);
            return ret;
        }

        //  - 마지막 aes128_padding_size 바이트는 패딩 영역
        if (aes128_padding_size > 0)
        {
            fr = f_write(&g_tdc_fs_ohdl, last_cipher + remain + 4, (UINT) aes128_padding_size, &bw);
            if (fr != FR_OK || (int) bw != aes128_padding_size)
            {
                TDC_PRINTF_E("[FS] WRITE PAD FAIL '%s' (res=%d, wrote=%u)\r\n", p_name, fr, bw);
                f_close(&g_tdc_fs_ohdl);
                return ret;
            }
        }
    }

    // 안전을 위한 flush
    f_sync(&g_tdc_fs_ohdl);

#if 1
    FILINFO fno;
    fr = f_stat(p_name, &fno);
    if (fr == FR_OK)
    {
        TDC_PRINTF_V("[FS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                  fno.fname,
                  fno.fsize,
                  g_tdc_fs_ohdl.obj.sclust);
    }
#endif

    f_close(&g_tdc_fs_ohdl);

#if 0
    {
        SEGGER_RTT_printf(0, "\r\n");

        if (enable_aes)
        {
            TDC_PRINTF("[FS] '%s' AFTER ENCRYPTION \r\n\n", p_name);
        }
        else
        {
            TDC_PRINTF("[FS] '%s' AFTER ENCRYPTION (ACTUALLY AES128 NOT USED) \r\n\n", p_name);
        }
        TDC_PRINTF("[FS] DATA \r\n");

        for (int print_i = 0; print_i < data_size; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", p_data[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == data_size))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (enable_crc)
        {
            TDC_PRINTF("[FS] CRC \r\n");
        }
        else
        {
            TDC_PRINTF("[FS] CRC (ACTUALLY CRC NOT USED) \r\n");
        }

        for (int print_i = 0; print_i < 4; print_i++)
        {
            SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_crc)[print_i]);

            if (((print_i + 1) % 32 == 0) || ((print_i + 1) == 4))
            {
                SEGGER_RTT_printf(0, "\r\n");
            }
        }

        SEGGER_RTT_printf(0, "\r\n");

        if (aes128_padding_size > 0)
        {
            TDC_PRINTF("[FS] PADDING \r\n");

            for (int print_i = 0; print_i < aes128_padding_size; print_i++)
            {
                SEGGER_RTT_printf(0, "%02X ", ((uint8_t*) p_uint32_aes128_padding)[print_i]);

                if (((print_i + 1) % 32 == 0) || ((print_i + 1) == aes128_padding_size))
                {
                    SEGGER_RTT_printf(0, "\r\n");
                }
            }

            SEGGER_RTT_printf(0, "\r\n");
        }
    }
#endif

    return 0;
}

int tdc_fs_read(char *p_name, uint8_t *p_buf, int size)
{
    int ret;
    int read;

    ret = f_open(&g_tdc_fs_ohdl, p_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);

    if (ret != FR_OK)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO OPEN '%s' (FRESULT=%d) \r\n", p_name, ret);
#endif
        return -1;
    }

    f_lseek(&g_tdc_fs_ohdl, 0);

    ret = f_read(&g_tdc_fs_ohdl, p_buf, size, &read);

    f_close(&g_tdc_fs_ohdl);

    if (ret != FR_OK)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO READ '%s' (FRESULT=%d) \r\n", p_name, ret);
#endif
        return -1;
    }

    if (size != read)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO READ '%s', NOT EQUAL SIZE(%d) AND READ(%d) \r\n", p_name, size, read);
#endif
        return -1;
    }

    return 0;
}

int tdc_fs_write(char *p_name, uint8_t *p_buf, int size)
{
    int ret;
    int written;

    ret = f_open(&g_tdc_fs_ohdl, p_name, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    if (ret != FR_OK)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO WRITE '%s' (FRESULT=%d) \r\n", p_name, ret);
#endif
        return -1;
    }

    f_lseek(&g_tdc_fs_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_tdc_fs_ohdl, 0);
    TDC_PRINTF_V("[FS] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", p_name);

    ret = f_write(&g_tdc_fs_ohdl, p_buf, size, &written);

    // 안전을 위한 flush
    f_sync(&g_tdc_fs_ohdl);

#if 1
    FILINFO fno;
    ret = f_stat(p_name, &fno);
    if (ret == FR_OK)
    {
        TDC_PRINTF_V("[FS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                  fno.fname,
                  fno.fsize,
                  g_tdc_fs_ohdl.obj.sclust);
    }
#endif

    f_close(&g_tdc_fs_ohdl);

    if (ret != FR_OK)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO WRITE '%s' (FRESULT=%d) \r\n", p_name, ret);
#endif
        return -1;
    }

    if (size != written)
    {
#if ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE
        TDC_PRINTF_E("[FS] FAILED TO WRITE '%s', NOT EQUAL SIZE(%d) AND WRITTEN(%d) \r\n", p_name, size, written);
#endif
        return -1;
    }

    return 0;
}

int tdc_fs_copy_isd_info_from_filesystem_to_shared_memory(void)
{
    int *p_src, *p_dst;

    for (int i = 0; i < MaxNumUser; i++)
    {
        p_dst = (int*) &(cfx_cm3_sharedMemoryAll.cfx_ISD_info[i]);
        p_src = (int*) &(g_tdc_fs_ptr_entire_map->map[i].isd_info);

        for (int k = 0; k < df_24bitWordLength_ISD_info; k++)
        {
            p_dst[k] = p_src[k];
        }
    }
}
