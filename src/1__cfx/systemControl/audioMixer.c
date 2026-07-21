/**
 * @file audioMixer.c
 */

#include <audioMixer.h>

/* ============================================================================
 * [MODULE] M1 믹싱 연산 모듈 - 입력 버퍼(들)을 HEAR_ADDR_AUDIO_MIX[16]로 합성.
 *   구성 유닛: U1 tdc_audio_mix_2_buffers_for_beamforming(DAS 빔포밍),
 *             U2 tdc_audio_mix_1_buffer(단일), U3 tdc_audio_mix_2_buffers(독립2버퍼),
 *             U13 tdc_audio_gain_lookup_q8_16(게인 테이블),
 *             U14 tdc_audio_mix_2_buffers_with_gain(게인 적용 2버퍼).
 *   검증: 유닛테스트(온타깃) - 입력 인수 결정론 → 출력 HEAR_ADDR 관측.
 *   전제(의존): U14는 U13 통과 전제.   상세: 유닛-모듈-테스트맵.md
 * ========================================================================== */

/* [UNIT] U13 게인 변환 테이블 - 인덱스(0~255) -> Q8.16 리니어 게인.
 *   인덱스 0 = 뮤트, 128 = 유니티(65536 = 1.0), 255 = +17.8594 dB.
 *   0.140625 dB 등간격으로 채워져 있으나 수식이 아닌 순수 룩업이다.
 *   특정 인덱스만 임의 게인으로 교체해 쓸 수 있게 인덱스별로 나열한다.
 *   출처: docs/참고/Gain Conversion Table/Gain Table.md
 *   검증=unit-test / 의존=없음 / 테스트벡터: 인덱스 0·128·255 룩업 -> 0·65536·512211. */
int chess_storage(XMEM) m_tdc_gain_table_q8_16[TDC_GAIN_TABLE_SIZE] = {
    0,      // [  0] mute
    8385,   // [  1] -17.8594 dB
    8522,   // [  2] -17.7188 dB
    8661,   // [  3] -17.5781 dB
    8802,   // [  4] -17.4375 dB
    8946,   // [  5] -17.2969 dB
    9092,   // [  6] -17.1563 dB
    9241,   // [  7] -17.0156 dB
    9391,   // [  8] -16.8750 dB
    9545,   // [  9] -16.7344 dB
    9700,   // [ 10] -16.5938 dB
    9859,   // [ 11] -16.4531 dB
    10020,  // [ 12] -16.3125 dB
    10183,  // [ 13] -16.1719 dB
    10349,  // [ 14] -16.0313 dB
    10518,  // [ 15] -15.8906 dB
    10690,  // [ 16] -15.7500 dB
    10865,  // [ 17] -15.6094 dB
    11042,  // [ 18] -15.4688 dB
    11222,  // [ 19] -15.3281 dB
    11405,  // [ 20] -15.1875 dB
    11591,  // [ 21] -15.0469 dB
    11781,  // [ 22] -14.9063 dB
    11973,  // [ 23] -14.7656 dB
    12168,  // [ 24] -14.6250 dB
    12367,  // [ 25] -14.4844 dB
    12569,  // [ 26] -14.3438 dB
    12774,  // [ 27] -14.2031 dB
    12982,  // [ 28] -14.0625 dB
    13194,  // [ 29] -13.9219 dB
    13410,  // [ 30] -13.7813 dB
    13629,  // [ 31] -13.6406 dB
    13851,  // [ 32] -13.5000 dB
    14077,  // [ 33] -13.3594 dB
    14307,  // [ 34] -13.2188 dB
    14540,  // [ 35] -13.0781 dB
    14778,  // [ 36] -12.9375 dB
    15019,  // [ 37] -12.7969 dB
    15264,  // [ 38] -12.6563 dB
    15513,  // [ 39] -12.5156 dB
    15766,  // [ 40] -12.3750 dB
    16024,  // [ 41] -12.2344 dB
    16285,  // [ 42] -12.0938 dB
    16551,  // [ 43] -11.9531 dB
    16821,  // [ 44] -11.8125 dB
    17096,  // [ 45] -11.6719 dB
    17375,  // [ 46] -11.5313 dB
    17658,  // [ 47] -11.3906 dB
    17947,  // [ 48] -11.2500 dB
    18239,  // [ 49] -11.1094 dB
    18537,  // [ 50] -10.9688 dB
    18840,  // [ 51] -10.8281 dB
    19147,  // [ 52] -10.6875 dB
    19460,  // [ 53] -10.5469 dB
    19777,  // [ 54] -10.4063 dB
    20100,  // [ 55] -10.2656 dB
    20428,  // [ 56] -10.1250 dB
    20762,  // [ 57] -9.9844 dB
    21100,  // [ 58] -9.8438 dB
    21445,  // [ 59] -9.7031 dB
    21795,  // [ 60] -9.5625 dB
    22151,  // [ 61] -9.4219 dB
    22512,  // [ 62] -9.2813 dB
    22880,  // [ 63] -9.1406 dB
    23253,  // [ 64] -9.0000 dB
    23633,  // [ 65] -8.8594 dB
    24018,  // [ 66] -8.7188 dB
    24410,  // [ 67] -8.5781 dB
    24809,  // [ 68] -8.4375 dB
    25214,  // [ 69] -8.2969 dB
    25625,  // [ 70] -8.1563 dB
    26043,  // [ 71] -8.0156 dB
    26469,  // [ 72] -7.8750 dB
    26901,  // [ 73] -7.7344 dB
    27340,  // [ 74] -7.5938 dB
    27786,  // [ 75] -7.4531 dB
    28239,  // [ 76] -7.3125 dB
    28700,  // [ 77] -7.1719 dB
    29169,  // [ 78] -7.0313 dB
    29645,  // [ 79] -6.8906 dB
    30129,  // [ 80] -6.7500 dB
    30620,  // [ 81] -6.6094 dB
    31120,  // [ 82] -6.4688 dB
    31628,  // [ 83] -6.3281 dB
    32144,  // [ 84] -6.1875 dB
    32669,  // [ 85] -6.0469 dB
    33202,  // [ 86] -5.9063 dB
    33744,  // [ 87] -5.7656 dB
    34295,  // [ 88] -5.6250 dB
    34855,  // [ 89] -5.4844 dB
    35424,  // [ 90] -5.3438 dB
    36002,  // [ 91] -5.2031 dB
    36589,  // [ 92] -5.0625 dB
    37187,  // [ 93] -4.9219 dB
    37794,  // [ 94] -4.7813 dB
    38410,  // [ 95] -4.6406 dB
    39037,  // [ 96] -4.5000 dB
    39674,  // [ 97] -4.3594 dB
    40322,  // [ 98] -4.2188 dB
    40980,  // [ 99] -4.0781 dB
    41649,  // [100] -3.9375 dB
    42329,  // [101] -3.7969 dB
    43020,  // [102] -3.6563 dB
    43722,  // [103] -3.5156 dB
    44435,  // [104] -3.3750 dB
    45161,  // [105] -3.2344 dB
    45898,  // [106] -3.0938 dB
    46647,  // [107] -2.9531 dB
    47408,  // [108] -2.8125 dB
    48182,  // [109] -2.6719 dB
    48969,  // [110] -2.5313 dB
    49768,  // [111] -2.3906 dB
    50580,  // [112] -2.2500 dB
    51406,  // [113] -2.1094 dB
    52245,  // [114] -1.9688 dB
    53097,  // [115] -1.8281 dB
    53964,  // [116] -1.6875 dB
    54845,  // [117] -1.5469 dB
    55740,  // [118] -1.4063 dB
    56650,  // [119] -1.2656 dB
    57574,  // [120] -1.1250 dB
    58514,  // [121] -0.9844 dB
    59469,  // [122] -0.8438 dB
    60440,  // [123] -0.7031 dB
    61426,  // [124] -0.5625 dB
    62429,  // [125] -0.4219 dB
    63448,  // [126] -0.2813 dB
    64484,  // [127] -0.1406 dB
    65536,  // [128] 0.0000 dB
    66606,  // [129] +0.1406 dB
    67693,  // [130] +0.2813 dB
    68798,  // [131] +0.4219 dB
    69921,  // [132] +0.5625 dB
    71062,  // [133] +0.7031 dB
    72222,  // [134] +0.8438 dB
    73400,  // [135] +0.9844 dB
    74598,  // [136] +1.1250 dB
    75816,  // [137] +1.2656 dB
    77054,  // [138] +1.4063 dB
    78311,  // [139] +1.5469 dB
    79589,  // [140] +1.6875 dB
    80888,  // [141] +1.8281 dB
    82209,  // [142] +1.9688 dB
    83550,  // [143] +2.1094 dB
    84914,  // [144] +2.2500 dB
    86300,  // [145] +2.3906 dB
    87709,  // [146] +2.5313 dB
    89140,  // [147] +2.6719 dB
    90595,  // [148] +2.8125 dB
    92074,  // [149] +2.9531 dB
    93577,  // [150] +3.0938 dB
    95104,  // [151] +3.2344 dB
    96656,  // [152] +3.3750 dB
    98234,  // [153] +3.5156 dB
    99837,  // [154] +3.6563 dB
    101467, // [155] +3.7969 dB
    103123, // [156] +3.9375 dB
    104806, // [157] +4.0781 dB
    106517, // [158] +4.2188 dB
    108255, // [159] +4.3594 dB
    110022, // [160] +4.5000 dB
    111818, // [161] +4.6406 dB
    113643, // [162] +4.7813 dB
    115498, // [163] +4.9219 dB
    117383, // [164] +5.0625 dB
    119299, // [165] +5.2031 dB
    121246, // [166] +5.3438 dB
    123225, // [167] +5.4844 dB
    125236, // [168] +5.6250 dB
    127280, // [169] +5.7656 dB
    129358, // [170] +5.9063 dB
    131469, // [171] +6.0469 dB
    133615, // [172] +6.1875 dB
    135796, // [173] +6.3281 dB
    138012, // [174] +6.4688 dB
    140265, // [175] +6.6094 dB
    142554, // [176] +6.7500 dB
    144881, // [177] +6.8906 dB
    147246, // [178] +7.0313 dB
    149649, // [179] +7.1719 dB
    152091, // [180] +7.3125 dB
    154574, // [181] +7.4531 dB
    157097, // [182] +7.5938 dB
    159661, // [183] +7.7344 dB
    162267, // [184] +7.8750 dB
    164915, // [185] +8.0156 dB
    167607, // [186] +8.1563 dB
    170343, // [187] +8.2969 dB
    173123, // [188] +8.4375 dB
    175949, // [189] +8.5781 dB
    178821, // [190] +8.7188 dB
    181739, // [191] +8.8594 dB
    184706, // [192] +9.0000 dB
    187720, // [193] +9.1406 dB
    190784, // [194] +9.2813 dB
    193898, // [195] +9.4219 dB
    197063, // [196] +9.5625 dB
    200279, // [197] +9.7031 dB
    203548, // [198] +9.8438 dB
    206871, // [199] +9.9844 dB
    210247, // [200] +10.1250 dB
    213679, // [201] +10.2656 dB
    217166, // [202] +10.4063 dB
    220711, // [203] +10.5469 dB
    224313, // [204] +10.6875 dB
    227974, // [205] +10.8281 dB
    231695, // [206] +10.9688 dB
    235477, // [207] +11.1094 dB
    239321, // [208] +11.2500 dB
    243227, // [209] +11.3906 dB
    247197, // [210] +11.5313 dB
    251231, // [211] +11.6719 dB
    255332, // [212] +11.8125 dB
    259499, // [213] +11.9531 dB
    263735, // [214] +12.0938 dB
    268039, // [215] +12.2344 dB
    272414, // [216] +12.3750 dB
    276861, // [217] +12.5156 dB
    281379, // [218] +12.6563 dB
    285972, // [219] +12.7969 dB
    290640, // [220] +12.9375 dB
    295383, // [221] +13.0781 dB
    300205, // [222] +13.2188 dB
    305105, // [223] +13.3594 dB
    310084, // [224] +13.5000 dB
    315146, // [225] +13.6406 dB
    320289, // [226] +13.7813 dB
    325517, // [227] +13.9219 dB
    330830, // [228] +14.0625 dB
    336230, // [229] +14.2031 dB
    341718, // [230] +14.3438 dB
    347295, // [231] +14.4844 dB
    352964, // [232] +14.6250 dB
    358725, // [233] +14.7656 dB
    364580, // [234] +14.9063 dB
    370530, // [235] +15.0469 dB
    376578, // [236] +15.1875 dB
    382724, // [237] +15.3281 dB
    388971, // [238] +15.4688 dB
    395320, // [239] +15.6094 dB
    401772, // [240] +15.7500 dB
    408330, // [241] +15.8906 dB
    414995, // [242] +16.0313 dB
    421768, // [243] +16.1719 dB
    428652, // [244] +16.3125 dB
    435648, // [245] +16.4531 dB
    442759, // [246] +16.5938 dB
    449986, // [247] +16.7344 dB
    457330, // [248] +16.8750 dB
    464795, // [249] +17.0156 dB
    472381, // [250] +17.1563 dB
    480091, // [251] +17.2969 dB
    487927, // [252] +17.4375 dB
    495891, // [253] +17.5781 dB
    503985, // [254] +17.7188 dB
    512211, // [255] +17.8594 dB
};

int tdc_audio_gain_lookup_q8_16(int gain_table_index)
{
    // 범위를 벗어난 인덱스는 유니티로 처리한다(무음·폭주 대신 현행 레벨 유지).
    if ((gain_table_index < 0) || (TDC_GAIN_TABLE_SIZE <= gain_table_index))
    {
        return m_tdc_gain_table_q8_16[TDC_GAIN_TABLE_INDEX_UNITY];
    }

    return m_tdc_gain_table_q8_16[gain_table_index];
}

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

/* [UNIT] U14 게인 적용 2버퍼 믹서 - U3(독립2버퍼)에 Q8.16 게인을 곱해 합산.
 *   p_mic_buf 에 gain_a(Gain_A), p_i2s_buf 에 gain_b(Gain_B)를 적용한다.
 *   두 곱을 48비트 long 으로 누산한 뒤 한 번만 >>16 하여 반올림 오차를 줄인다.
 *   24비트를 넘는 결과는 랩어라운드(부호 반전 -> 폭발적 잡음) 대신 절삭(포화)한다.
 *   유니티 게인(65536)이면 (x << 16) >> 16 = x 로 U3 와 비트 단위로 동일하다.
 *   검증=unit-test / 의존=U13(게인 테이블) 통과 전제 /
 *   테스트벡터: 함수 시작점 p_mic_buf·p_i2s_buf·게인 하드코딩 → HEAR 관측. */
void tdc_audio_mix_2_buffers_with_gain(int _XMEM *p_mic_buf, int _XMEM *p_i2s_buf, int gain_a_q8_16, int gain_b_q8_16)
{
    long       gained_value;
    int _XMEM *p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;

    // 입력 오디오 : Q24.0 (>>AUDIO_INPUT_RSHIFT 로 입력 스케일 적용)
    // 게인        : Q8.16
    // 곱의 누산   : Q32.16
    // 믹싱 출력   : Q24.0

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            gained_value = ((long) gain_a_q8_16) * (p_mic_buf[i] >> AUDIO_INPUT_RSHIFT);
            gained_value = gained_value + (((long) gain_b_q8_16) * (p_i2s_buf[i] >> AUDIO_INPUT_RSHIFT));
            gained_value = gained_value >> TDC_GAIN_Q8_16_SHIFT;

            // 절삭(포화) - 24비트를 넘으면 최대·최소값에 머문다.
            if (gained_value > INT24_MAX)
            {
                gained_value = INT24_MAX;
            }
            else if (gained_value < INT24_MIN)
            {
                gained_value = INT24_MIN;
            }

            p_mix[i] = (int) gained_value;
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
