/*
 * [공유 ABI - 단독 rename/제거 금지] (2026-07-23, cm3 2차 리팩토링 G1)
 *
 * FS_MEM_UART 는 이름과 달리 UART 하드웨어와 무관하다. CM3 <-> CFX 가
 * DSP PRAM1 뱅크(DSP_PRAM1_REMAP_BASE)를 공유해 주고받는 디버그 채널이다.
 * CFX(1__cfx/OTE_1_5_gen/OTE_1_5_gen_UART.h)가 동일 레이아웃 사본을 갖는다.
 *
 *  - 필드(state / flag[32] / buffer[512])의 이름·순서·타입·크기 변경 = 레이아웃(ABI) 파괴 -> CFX 오동작
 *  - 베이스 주소 매크로(DSP_PRAM1_REMAP_BASE)는 SDK 헤더가 제공하며, CFX 의 D_DSP_PRAM1_BASE 와 같은 뱅크를 가리킨다
 *
 * 살아있는 경로: CM3 tdc_shm.c 가 flag[0]=1 로 계산 완료를 알리면,
 *   CFX nonlinearMapping.c 가 buffer[] 에 로그매핑 계수를 채운 뒤 flag[0]=2 로 응답한다.
 *
 * 2026-07-23: hal/tdc_hal_uart.h 에서 이 헤더로 분리 이관. UART 하드웨어 코드 제거(G2)와 분리하기 위함.
 * 확장 필드(CFX 사본에도 없고 양쪽 #if 0)는 이관하지 않았다.
 */

#ifndef __tdc_shm_debug_h__
#define __tdc_shm_debug_h__

#include <hw.h>

#define FS_MEM_UART_STATE_RESET 0x00
#define FS_MEM_UART_STATE_INIT  0x11
#define FS_MEM_UART_STATE_IDLE  0x22
#define FS_MEM_UART_STATE_CM3   0x33
#define FS_MEM_UART_STATE_CFX   0x44

typedef struct
{
    int state;
    int flag[32];
    int buffer[512];
} FS_MEM_UART_T;

#define FS_MEM_UART ((volatile FS_MEM_UART_T*) DSP_PRAM1_REMAP_BASE)

#endif  // __tdc_shm_debug_h__
