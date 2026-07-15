/**
 * @file audioMixer.c
 */

#include <audioMixer.h>

/* ============================================================================
 * [MODULE] M1 믹싱 연산 모듈 - 입력 버퍼(들)을 HEAR_ADDR_AUDIO_MIX[16]로 합성.
 *   구성 유닛: U1 tdc_audio_mix_2_buffers_for_beamforming(DAS 빔포밍),
 *             U2 tdc_audio_mix_1_buffer(단일), U3 tdc_audio_mix_2_buffers(독립2버퍼).
 *   검증: 유닛테스트(온타깃) - 입력 인수 결정론 → 출력 HEAR_ADDR 관측.
 *   전제(의존): 없음.   상세: 유닛-모듈-테스트맵.md
 * ========================================================================== */

/* [UNIT] U2 단일버퍼 믹서 - p_buf1[16]을 >>AUDIO_INPUT_RSHIFT 스케일해 HEAR로 복사.
 *   검증=unit-test / 의존=없음 / 테스트벡터: 함수 시작점 p_buf1 하드코딩 → HEAR 관측. */
void tdc_audio_mix_1_buffer(int _XMEM *p_buf1)
{
    int _XMEM *p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            p_mix[i] = (p_buf1[i] >> AUDIO_INPUT_RSHIFT);
        }
}

/* [UNIT] U3 독립2버퍼 믹서 - 독립 음원(mic + I2S) 단순 합산(÷2 없음).
 *   검증=unit-test / 의존=없음 / 테스트벡터: p_buf1·p_buf2 하드코딩 → HEAR 관측. */
void tdc_audio_mix_2_buffers(int _XMEM *p_buf1, int _XMEM *p_buf2)
{
    int _XMEM *p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            // 독립 음원(mic + I2S) 단순 합산 - 의도적으로 ÷2(>>1) 생략.
            // 빔포밍(동일 음원 2경로)과 달리 두 입력이 독립이므로 합산 레벨이 그대로 출력이 된다.
            p_mix[i] = ((p_buf1[i] >> AUDIO_INPUT_RSHIFT) + (p_buf2[i] >> AUDIO_INPUT_RSHIFT));
        }
}

/**
 * [UNIT] U1 DAS 빔포밍 믹서 (검증=unit-test / 의존=없음 /
 *        테스트벡터: 함수 시작점 3버퍼 하드코딩 → HEAR 관측. 케이스 (a)i=0..14 (b)경계 i=15)
 *
 * tdc_audio_mix_2_buffers_for_beamforming - delay-and-sum(DAS) 빔포밍 믹서.
 *
 * ===== 1) 마이크 배치와 소리 방향 =====================================
 *
 *      정면 음원  )))  ── 소리 진행 방향 ──>
 *
 *         +-------+       22 mm        +-------+
 *         | FRONT |<------------------>| REAR  |
 *         +-------+                    +-------+
 *        (delay 채널)                 (no_delay/기준 채널)
 *       음원에 먼저 도달          FRONT보다 ~1.025샘플 늦게 도달
 *
 *   어느 물리 마이크가 FRONT인지는 착용 귀로 결정(호출부에서 인수로 선택):
 *       Left  귀 → FRONT = DMIC2(QCC),  REAR = DMIC1(EZ)
 *       Right 귀 → FRONT = DMIC1(EZ),   REAR = DMIC2(QCC)
 *
 * ===== 2) 같은 소리가 두 마이크에 1샘플 차로 들어온다 =================
 *   음향 샘플(시간 순, 왼쪽이 과거):  A B C D E F G H I J ...
 *
 *      FRONT(먼저 들음) : ... E F G H I J    ← 항상 1샘플 앞섬
 *      REAR (늦게 들음) : ... D E F G H I     ← FRONT보다 1샘플 뒤
 *                                       ▲
 *               같은 소리 'I' :  REAR 최신 == FRONT 직전 샘플
 *
 * ===== 3) FIFO→버퍼 저장과 '인덱스 1칸' 지연 (sample index 0=최신) ====
 *   매 인터럽트마다 16샘플 블록이 FIFO로 들어와 버퍼[mic][block][sample]에
 *   저장된다(block0=현재, block1=직전 / sample0=최신 … 15=과거).
 *
 *      idx              :   0     1     2     3   ...
 *      no_delay(REAR)   : [ I ] [ H ] [ G ] [ F ] ...  ← p_no_delay_buf0
 *      delay   (FRONT)  : [ J ] [ I ] [ H ] [ G ] ...  ← p_delay_buf0
 *                                 └───┐
 *                                     ▼  delay를 i+1(한 칸 과거)에서 읽음 = 1샘플 지연
 *      p_mix[i] = ( no_delay[i] + delay[i+1] ) >> AUDIO_MIX_NORMALIZE_RSHIFT
 *      p_mix[0] = REAR[0]=I + FRONT[1]=I = 2·I  → 정면 소리 보강(상관 합)
 *      p_mix[1] = REAR[1]=H + FRONT[2]=H = 2·H  → ...
 *
 *   블록 경계(i=15): delay_buf0[16]은 같은 블록에 없으므로
 *                    직전 블록의 최신 샘플 delay_buf1[0] 으로 이어붙인다.
 *
 * ===== 4) 총 지연 1.025샘플 = SW 1.000 + HW 0.025 =====================
 *   - SW(이 함수)            : delay 채널 i+1 인덱스 시프트   → 정수 1.000 샘플
 *   - HW(decimation, FRONT) : DELAY_INTEGER=0, DELAY_FRACTIONAL=6
 *         = 6/240 샘플 = 0.025 샘플 (서브샘플 보정).
 *         system_control.c의 L/R 분기에서 FRONT 채널에만
 *         LIB_ADC_DEC_CTRL_VAL_0_0250 으로 설정(REAR 채널은 지연 0).
 *   - 합계 : 1.025 샘플 ~ 64.06 us ~ 22 mm / 343 m·s  (end-fire 정면 정렬)
 *
 * 인수: p_delay_buf0=FRONT 현재블록, p_delay_buf1=FRONT 직전블록,
 *       p_no_delay_buf0=REAR 현재블록. 각 채널 >>AUDIO_INPUT_RSHIFT(입력 스케일)
 *       후 합산, >>AUDIO_MIX_NORMALIZE_RSHIFT(=1)로 2채널 평균 정규화.
 */
void tdc_audio_mix_2_buffers_for_beamforming(int _XMEM *p_delay_buf0, int _XMEM *p_delay_buf1, int _XMEM *p_no_delay_buf0)
{
    int _XMEM *p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;

    p_mix[0]  = ((p_no_delay_buf0[0] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[1] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[1]  = ((p_no_delay_buf0[1] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[2] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[2]  = ((p_no_delay_buf0[2] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[3] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[3]  = ((p_no_delay_buf0[3] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[4] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[4]  = ((p_no_delay_buf0[4] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[5] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[5]  = ((p_no_delay_buf0[5] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[6] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[6]  = ((p_no_delay_buf0[6] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[7] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[7]  = ((p_no_delay_buf0[7] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[8] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[8]  = ((p_no_delay_buf0[8] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[9] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[9]  = ((p_no_delay_buf0[9] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[10] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[10] = ((p_no_delay_buf0[10] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[11] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[11] = ((p_no_delay_buf0[11] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[12] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[12] = ((p_no_delay_buf0[12] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[13] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[13] = ((p_no_delay_buf0[13] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[14] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[14] = ((p_no_delay_buf0[14] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf0[15] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
    p_mix[15] = ((p_no_delay_buf0[15] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf1[0] >> AUDIO_INPUT_RSHIFT)) >> AUDIO_MIX_NORMALIZE_RSHIFT;
}
/* eof */
