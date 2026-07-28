#ifndef __stub_ble_h__
#define __stub_ble_h__

#include <stdint.h>
#include <tdc_ble_mapping.h>

// ble/mapping/ 파싱 함수가 의존하는 프로덕션 심볼의 테스트 대역.
//
// 스텁 자체는 원래 이름 그대로 정의한다(링크 대체이므로 필연).
// 테스트가 상태를 들여다보는 조회 API 만 stub_ 접두어를 쓴다.
//
// tdc_ble_reply_* 는 스텁이 아니다. 응답 조립 결과까지 검증 대상이라
// 실물(src/2__cm3/source/ble/tdc_ble_reply.c)을 함께 컴파일한다.

// 모든 스텁 상태를 초기값으로 되돌린다. 각 테스트 케이스 앞에서 부른다.
void stub_reset(void);

// ---- 에러 응답 (tdc_sys_error_send_to_app) ----
int stub_error_count(void);
int stub_error_last_command(void);
int stub_error_last_major(void);
int stub_error_last_minor(void);
// 라인 번호는 소스가 한 줄만 밀려도 바뀌므로 단언 대상이 아니다.
// 디버깅용으로만 노출한다.
int stub_error_last_line(void);

// ---- SPI 송신 (tdc_hal_spi_write_tx_buffer) ----
// 이 인프라의 핵심. 실기에서 로그로만 보던 응답 바이트열을
// 오프라인에서 바이트 단위로 단언할 수 있다.
int            stub_tx_count(void);   // 송신 호출 횟수
int            stub_tx_len(void);     // 마지막 송신 길이
const uint8_t *stub_tx_buffer(void);  // 마지막 송신 내용

// ---- 공유 메모리 저장소 ----
int *stub_isd_info_repository(void);     // read_write_map_data_isd_info
int *stub_stimul_para_repository(void);  // read_write_map_data_stimul_para

// 저장소 경계를 넘어 쓰면 테스트가 알 수 있도록 표식을 남긴다.
int stub_repository_overflow(void);

#endif  // __stub_ble_h__
