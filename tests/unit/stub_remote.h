#ifndef __stub_remote_h__
#define __stub_remote_h__

#include <stdbool.h>
#include <stdint.h>

// ble/remote/ 파싱 함수가 의존하는 프로덕션 심볼의 테스트 대역.
//
// stub_ble.c 와 역할이 갈린다.
//   stub_ble.c    : ble/mapping/ 공용 (에러 응답 · SPI 송신 · 맵 저장소)
//   stub_remote.c : 리모콘 전용 (공유메모리 설정값 R/W · 플래시 명령 · 기타)
//
// 리모콘은 "읽고 -> 바꾸고 -> 되읽어 응답" 패턴을 쓰므로, 공유메모리 스텁은
// 값을 실제로 보관해야 의미 있는 테스트가 된다. 단순히 0 을 돌려주면
// 볼륨 변경 같은 명령의 검증이 성립하지 않는다.
//
// 스텁 자체는 원래 이름 그대로 정의한다(링크 대체이므로 필연).
// 테스트가 상태를 들여다보는 조회 API 만 stub_remote_ 접두어를 쓴다.

// 모든 리모콘 스텁 상태를 초기값으로 되돌린다.
// stub_reset() 과 짝으로 각 테스트 케이스 앞에서 부른다.
void stub_remote_reset(void);

// ---- 공유메모리 설정값 (읽고 쓰는 값이 유지된다) ----
int stub_remote_get_stimul_volume(void);
int stub_remote_get_audio_volume(void);
int stub_remote_get_program_map_num(void);
int stub_remote_get_tele_coil_on_off(void);
int stub_remote_get_stimul_indicator_on_off(void);
int stub_remote_get_led_indicator_on_off(void);

// 테스트가 초기 상태를 만들 때 쓴다.
void stub_remote_set_stimul_volume(int v);
void stub_remote_set_audio_volume(int v);
void stub_remote_set_program_map_num(int v);
void stub_remote_set_usable_map_num(int v);

// 사용 가능한 맵 인덱스 배열 (0 이면 사용 불가). 테스트가 직접 채운다.
int *stub_remote_usable_map_index(void);

// ---- 플래시 명령 호출 기록 ----
// 어떤 플래시 동작이 요청됐는지만 본다. 실제 NVM 접근은 하지 않는다.
int stub_remote_flash_call_count(void);
int stub_remote_flash_last_command(void);   // 마지막 호출의 command 인자
int stub_remote_flash_last_slot(void);      // 마지막 호출의 slot_index
int stub_remote_flash_last_map(void);       // 마지막 호출의 map_index (없으면 -1)

// ---- 기타 ----
int stub_remote_battery_percent(void);
void stub_remote_set_battery_percent(int pct);

int stub_remote_stim_mute_call_count(void);
int stub_remote_stim_mute_last_enable(void);
int stub_remote_stim_mute_last_level(void);

int stub_remote_event_log_write_count(void);
int stub_remote_event_log_last_type(void);

#endif  // __stub_remote_h__
