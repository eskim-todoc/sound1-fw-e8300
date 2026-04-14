/**
 * @file lib_i2s_bridge.c
 */

#include <lib_i2s_bridge.h>

#include <chess.h>

// ====== 플랫폼 한계/범위(24비트 signed) ======
#define SAT_MIN (-8388608)
#define SAT_MAX (8388607)

// ====== Q8.16 유틸 ======
#define Q16_ONE      (65535u)
#define Q16_CLAMP(x) ((x) > (long) Q16_ONE ? (long) Q16_ONE : ((x) < 0 ? 0 : (x)))

// 부호 보존 반올림 나눗셈: x/65535
static inline long div65535_round_long(long x)
{
    if (x >= 0)
        return (x + 32767) / 65535;
    else
        return -(((-x) + 32767) / 65535);
}

// Q8.16 곱: (a*b)/65535 (반올림)
static inline unsigned int q16_mul(unsigned int a, unsigned int b)
{
    long p = (long) a * (long) b;  // 24x24 -> 48비트(long)
    return (unsigned int) ((p + 32767) / 65535);
}

// 24비트 포화
static inline int sat24(long x)
{
    if (x > SAT_MAX)
        return SAT_MAX;
    if (x < SAT_MIN)
        return SAT_MIN;
    return (int) x;
}

// median-of-3 (기울기 추정용)
static inline int median3_int(int a, int b, int c)
{
    if ((a >= b && a <= c) || (a <= b && a >= c))
        return a;
    if ((b >= a && b <= c) || (b <= a && b >= c))
        return b;
    return c;
}

// ========== C1 Hermite 브리지 생성 ==========
// 입력:  D[16], E[16]  (블록 내부 재생순서: [15]→…→[0])
// 출력:  S1[16], S2[16] (S1[15]=D[0], S2[0]=E[15])
void build_bridge_C1_Q816_cfx(const int D[16], const int E[16], int S1[16], int S2[16])
{
    // (선택) RSS 모드 설정: 수렴 라운딩 + 새추레이션
    // set_rounding_mode(3);     // 0:trunc,1:sign-mag trunc,2:biased round,3:convergent round
    // set_saturation_mode(1);   // 0:wrap, 1:saturate

    const int d0  = D[0];   // D의 마지막
    const int e15 = E[15];  // E의 첫 샘플

    // 1) 기울기(차분) 추정: [15]→…→[0] 인덱스 체계 반영
    //    D 쪽: D[0]-D[1], D[1]-D[2], D[2]-D[3]
    //    E 쪽: E[14]-E[15], E[13]-E[14], E[12]-E[13]
    int mD = median3_int(D[0] - D[1], D[1] - D[2], D[2] - D[3]);
    int mE = median3_int(E[14] - E[15], E[13] - E[14], E[12] - E[13]);

    // (옵션) 과도 오버슈트 방지용 클램프 (필요 시 상수 조정)
    // const int SLOPE_LIM = 0x004000; // 예) Q1.23 기준 ~0.5
    // if (mD >  SLOPE_LIM) mD =  SLOPE_LIM; else if (mD < -SLOPE_LIM) mD = -SLOPE_LIM;
    // if (mE >  SLOPE_LIM) mE =  SLOPE_LIM; else if (mE < -SLOPE_LIM) mE = -SLOPE_LIM;

    // 2) 32샘플 Hermite: t = n/31 (Q8.16), T=31 (스텝 수)
    //    y = h00*d0 + h01*e15 + (h10*T)*mD + (h11*T)*mE
    //    h00=2t^3-3t^2+1, h10=t^3-2t^2+t, h01=-2t^3+3t^2, h11=t^3-t^2
    int  n;
    long y32[32];

    for (n = 0; n < 32; n++)
    {
        // t in Q8.16: round(n*65535/31)
        unsigned int t  = (unsigned int) (((long) n * (long) Q16_ONE + 15) / 31);
        unsigned int t2 = q16_mul(t, t);
        unsigned int t3 = q16_mul(t2, t);

        long h00 = (long) 2 * (long) t3 - (long) 3 * (long) t2 + (long) Q16_ONE;
        long h10 = (long) t3 - (long) 2 * (long) t2 + (long) t;
        long h01 = -(long) 2 * (long) t3 + (long) 3 * (long) t2;
        long h11 = (long) t3 - (long) t2;

        // 0..65535 범위로 클램프
        h00 = Q16_CLAMP(h00);
        h10 = Q16_CLAMP(h10);
        h01 = Q16_CLAMP(h01);
        h11 = Q16_CLAMP(h11);

        // (h10*T), (h11*T) : T=31
        long h10T = h10 * 31;
        long h11T = h11 * 31;

        // 가중합 (모두 Q8.16 가중치) → 마지막에 /65535
        long acc = (long) d0 * h00;
        acc += (long) e15 * h01;
        acc += (long) mD * h10T;
        acc += (long) mE * h11T;

        // 반올림 나눗셈 & 24비트 포화
        long yn = div65535_round_long(acc);
        y32[n]  = yn;
    }

    // 3) S1/S2에 배치 (재생 순서 [15]→…→[0] 보정)
    for (n = 0; n < 16; n++)
        S1[15 - n] = sat24(y32[n]);                                      // S1[15]=y[0]=D[0]
    for (n = 16; n < 32; n++)     S2[15 - (n-16)] = sat24(y32[n]);       // S2[0] =y[31]=E[15]
}
