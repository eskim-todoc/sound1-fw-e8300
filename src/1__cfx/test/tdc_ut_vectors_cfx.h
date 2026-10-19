#ifndef TDC_UT_VECTORS_CFX_H
#define TDC_UT_VECTORS_CFX_H
#if defined(TDC_UNIT_TEST)
#define TDC_UT_CFX_VECTOR_CAPACITY 528
#define TDC_UT_CFX_NUMERIC_CAPACITY 400
int tdc_ut_cfx_run_unit(int unit);
/* -2 means previous immutable result has not been released after RTT drain. */
void tdc_ut_cfx_vectors_release(void);
extern int _XMEM g_tdc_ut_cfx_vector_actual[TDC_UT_CFX_VECTOR_CAPACITY];
extern int _XMEM g_tdc_ut_cfx_vector_expected[TDC_UT_CFX_VECTOR_CAPACITY];
extern int g_tdc_ut_cfx_vector_count, g_tdc_ut_cfx_vector_failed;
extern int g_tdc_ut_cfx_vector_overflow;
extern volatile int g_tdc_ut_cfx_vector_done;
/* CFX-023/031/032의 C10 이후는 SDK 경로의 허용 구간을 비교한다.
 * vector 배열의 1은 해당 구간 안에 있다는 뜻이다. 실제 계산값을 1로
 * 바꾸지 않으며 numeric 배열에 [실제값, 사전 하한, 사전 상한]을 남긴다.
 * numeric도 -999/unit/case/길이 형식이다. expected=NULL로 관찰 출력하고
 * vector의 구간 판정과 함께 보존한다. RTT 배출 전 release하지 않는다.
 */
extern int _XMEM g_tdc_ut_cfx_numeric_actual[TDC_UT_CFX_NUMERIC_CAPACITY];
extern int g_tdc_ut_cfx_numeric_count;
#endif
#endif
