#ifndef TDC_SYS_TEST_CONFIG_CFX_H
#define TDC_SYS_TEST_CONFIG_CFX_H

/* 시스템 시험용 DIO35 사용 여부: 0=사용 안 함, 1=CFX 시간 표시.
 * SYS-012는 CM3 processorDirective.h의 같은 상수도 1로 빌드해야 한다.
 * CM3는 초기화만 하고 측정 중에는 CFX만 이 핀을 제어한다.
 * 실제 보드에서 미사용 핀임을 확인한다. EXTCLK 입력과 함께 쓸 수 없다. */
#ifndef TDC_SYSTEM_TEST_DIO35_ENABLE
#define TDC_SYSTEM_TEST_DIO35_ENABLE 0
#endif

#if (TDC_SYSTEM_TEST_DIO35_ENABLE != 0) && (TDC_SYSTEM_TEST_DIO35_ENABLE != 1)
#error "TDC_SYSTEM_TEST_DIO35_ENABLE must be 0 or 1"
#endif

#endif /* TDC_SYS_TEST_CONFIG_CFX_H */
