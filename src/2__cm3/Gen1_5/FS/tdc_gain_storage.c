/*
 * tdc_gain_storage.c
 *
 * Gain Conversion Table 인덱스의 ISD별 영속화. 상세는 tdc_gain_storage.h 참조.
 */

#include <tdc_gain_storage.h>

#include <stdbool.h>

#include <ci_filesystem.h>
#include <ci_printf.h>

/* 쓰기 함수는 (데이터 % 16) + 4(CRC) + 패딩 == 16 을 만족하지 않으면 파일에
 * 아무것도 쓰지 않고 실패만 반환한다. 구조체가 바뀌어 이 조건이 깨지면
 * 런타임에 조용히 저장이 안 되므로, 컴파일 단계에서 막는다.
 * (조건 위반 시 아래 배열 크기가 음수가 되어 컴파일 에러) */
typedef char tdc_gain_file_alignment_check[(((sizeof(ST__TDC_GAIN_FILE) % 16) + 4 + TDC_GAIN_FILE_PADDING_LEN) == 16) ? 1 : -1];

// ci_filesystem 의 read/write 는 파일명을 char* 로 받으므로 배열로 둔다.
static char m_tdc_gain_file_name[] = TDC_GAIN_FILE_NAME;

static ST__TDC_GAIN_FILE m_tdc_gain_file;
static uint32_t          m_tdc_gain_crc_ccitt;
static uint32_t          m_tdc_gain_padding[TDC_GAIN_FILE_PADDING_LEN / 4];

// 파일 내용이 한 번이라도 메모리로 올라왔는지 (저장 시 다른 슬롯 보존에 필요)
static bool m_is_loaded = false;

static bool tdc_gain_is_valid_index(int index);
static void tdc_gain_set_default_all(void);
static int  tdc_gain_read_file(void);
static int  tdc_gain_write_file(void);
static int  tdc_gain_load_and_repair(void);

int tdc_gain_storage_init(void)
{
    int ret = tdc_gain_load_and_repair();

    ci_printi("[GAIN] STORAGE INIT : ISD1(%d,%d) ISD2(%d,%d) ISD3(%d,%d) ISD4(%d,%d) \r\n",  //
              m_tdc_gain_file.setting[0].gain_table_index_a, m_tdc_gain_file.setting[0].gain_table_index_b,
              m_tdc_gain_file.setting[1].gain_table_index_a, m_tdc_gain_file.setting[1].gain_table_index_b,
              m_tdc_gain_file.setting[2].gain_table_index_a, m_tdc_gain_file.setting[2].gain_table_index_b,
              m_tdc_gain_file.setting[3].gain_table_index_a, m_tdc_gain_file.setting[3].gain_table_index_b);

    return ret;
}

int tdc_gain_storage_load(int isd_num, ST__TDC_GAIN_SETTING *p_out)
{
    if (p_out == NULL)
    {
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    // 어떤 경로로 실패하더라도 호출부가 쓸 수 있도록 먼저 기본값을 채워 둔다.
    p_out->gain_table_index_a = TDC_GAIN_TABLE_INDEX_DEFAULT_A;
    p_out->gain_table_index_b = TDC_GAIN_TABLE_INDEX_DEFAULT_B;

    if ((isd_num < 1) || (MaxNumUser < isd_num))
    {
        ci_printw("[GAIN] LOAD FAIL (INVALID ISD NUM: %d) \r\n", isd_num);
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    if (tdc_gain_load_and_repair() != TDC_GAIN_STORAGE_RET_OK)
    {
        // 복구까지 실패한 경우로, 기본값을 그대로 쓴다.
        ci_printw("[GAIN] LOAD FAIL (ISD %d), USE DEFAULT \r\n", isd_num);
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    *p_out = m_tdc_gain_file.setting[isd_num - 1];

    ci_printi("[GAIN] LOADED : ISD %d, A %d, B %d \r\n", isd_num, p_out->gain_table_index_a, p_out->gain_table_index_b);

    return TDC_GAIN_STORAGE_RET_OK;
}

int tdc_gain_storage_save(int isd_num, const ST__TDC_GAIN_SETTING *p_in)
{
    if (p_in == NULL)
    {
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    if ((isd_num < 1) || (MaxNumUser < isd_num))
    {
        ci_printw("[GAIN] SAVE FAIL (INVALID ISD NUM: %d) \r\n", isd_num);
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    if (!tdc_gain_is_valid_index(p_in->gain_table_index_a) || !tdc_gain_is_valid_index(p_in->gain_table_index_b))
    {
        ci_printw("[GAIN] SAVE FAIL (INVALID INDEX: %d, %d) \r\n", p_in->gain_table_index_a, p_in->gain_table_index_b);
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    // 파일 하나에 전 슬롯이 들어 있으므로, 다른 슬롯 값을 잃지 않으려면
    // 먼저 파일 내용을 메모리에 올려 둔 뒤 해당 슬롯만 바꿔야 한다.
    if (!m_is_loaded)
    {
        tdc_gain_load_and_repair();
    }

    m_tdc_gain_file.setting[isd_num - 1] = *p_in;

    if (tdc_gain_write_file() != TDC_GAIN_STORAGE_RET_OK)
    {
        ci_printw("[GAIN] SAVE FAIL (WRITE ERROR, ISD %d) \r\n", isd_num);
        return TDC_GAIN_STORAGE_RET_FAIL;
    }

    ci_printi("[GAIN] SAVED : ISD %d, A %d, B %d \r\n", isd_num, p_in->gain_table_index_a, p_in->gain_table_index_b);

    return TDC_GAIN_STORAGE_RET_OK;
}

static bool tdc_gain_is_valid_index(int index)
{
    return ((TDC_GAIN_TABLE_INDEX_MIN <= index) && (index <= TDC_GAIN_TABLE_INDEX_MAX));
}

static void tdc_gain_set_default_all(void)
{
    m_tdc_gain_file.ident_begin = TDC_GAIN_FILE_IDENT_BEGIN;
    m_tdc_gain_file.ident_end   = TDC_GAIN_FILE_IDENT_END;

    for (int i = 0; i < MaxNumUser; i++)
    {
        m_tdc_gain_file.setting[i].gain_table_index_a = TDC_GAIN_TABLE_INDEX_DEFAULT_A;
        m_tdc_gain_file.setting[i].gain_table_index_b = TDC_GAIN_TABLE_INDEX_DEFAULT_B;
    }
}

static int tdc_gain_read_file(void)
{
    int ret;

    ret = ci_filesystem_read_with_crc_and_aes128(m_tdc_gain_file_name,          //
                                                 (uint8_t *) &m_tdc_gain_file,  //
                                                 sizeof(ST__TDC_GAIN_FILE),     //
                                                 &m_tdc_gain_crc_ccitt,         //
                                                 m_tdc_gain_padding,            //
                                                 TDC_GAIN_FILE_PADDING_LEN,     //
                                                 true,                          // CRC 사용
                                                 false);                        // AES 미사용

    return ((ret < 0) ? TDC_GAIN_STORAGE_RET_FAIL : TDC_GAIN_STORAGE_RET_OK);
}

static int tdc_gain_write_file(void)
{
    int ret;

    // 매직넘버는 항상 최신 값으로 유지한다.
    m_tdc_gain_file.ident_begin = TDC_GAIN_FILE_IDENT_BEGIN;
    m_tdc_gain_file.ident_end   = TDC_GAIN_FILE_IDENT_END;

    ret = ci_filesystem_write_with_crc_and_aes128(m_tdc_gain_file_name,          //
                                                  (uint8_t *) &m_tdc_gain_file,  //
                                                  sizeof(ST__TDC_GAIN_FILE),     //
                                                  &m_tdc_gain_crc_ccitt,         //
                                                  m_tdc_gain_padding,            //
                                                  TDC_GAIN_FILE_PADDING_LEN,     //
                                                  true,                          // CRC 사용
                                                  false);                        // AES 미사용

    return ((ret < 0) ? TDC_GAIN_STORAGE_RET_FAIL : TDC_GAIN_STORAGE_RET_OK);
}

/* 파일을 읽어 메모리에 올리고, 손상된 부분을 기본값으로 되돌린다.
 * 되돌린 내용이 있으면 파일에 다시 기록한다(자기 수복). */
static int tdc_gain_load_and_repair(void)
{
    bool need_rewrite = false;

    if (tdc_gain_read_file() != TDC_GAIN_STORAGE_RET_OK)
    {
        // 파일이 없거나 CRC 가 깨진 경우로, 전 슬롯을 기본값으로 만든다.
        ci_printw("[GAIN] FILE READ FAIL, RESET ALL SLOTS \r\n");
        tdc_gain_set_default_all();
        need_rewrite = true;
    }
    else if ((m_tdc_gain_file.ident_begin != TDC_GAIN_FILE_IDENT_BEGIN)  //
             || (m_tdc_gain_file.ident_end != TDC_GAIN_FILE_IDENT_END))
    {
        // 크기는 맞지만 우리 파일이 아닌 경우다.
        ci_printw("[GAIN] FILE IDENT MISMATCH, RESET ALL SLOTS \r\n");
        tdc_gain_set_default_all();
        need_rewrite = true;
    }
    else
    {
        // 슬롯 단위로 값을 확인해, 문제 있는 슬롯만 기본값으로 되돌린다.
        for (int i = 0; i < MaxNumUser; i++)
        {
            if (!tdc_gain_is_valid_index(m_tdc_gain_file.setting[i].gain_table_index_a))
            {
                ci_printw("[GAIN] INVALID INDEX A (SLOT %d: %d), RESET \r\n", i + 1, m_tdc_gain_file.setting[i].gain_table_index_a);
                m_tdc_gain_file.setting[i].gain_table_index_a = TDC_GAIN_TABLE_INDEX_DEFAULT_A;
                need_rewrite                                  = true;
            }

            if (!tdc_gain_is_valid_index(m_tdc_gain_file.setting[i].gain_table_index_b))
            {
                ci_printw("[GAIN] INVALID INDEX B (SLOT %d: %d), RESET \r\n", i + 1, m_tdc_gain_file.setting[i].gain_table_index_b);
                m_tdc_gain_file.setting[i].gain_table_index_b = TDC_GAIN_TABLE_INDEX_DEFAULT_B;
                need_rewrite                                  = true;
            }
        }
    }

    // 메모리 내용은 이 시점에서 항상 유효하다.
    m_is_loaded = true;

    if (need_rewrite)
    {
        if (tdc_gain_write_file() != TDC_GAIN_STORAGE_RET_OK)
        {
            ci_printw("[GAIN] REPAIR WRITE FAIL \r\n");
            return TDC_GAIN_STORAGE_RET_FAIL;
        }
    }

    return TDC_GAIN_STORAGE_RET_OK;
}
