---
name: 토론 페르소나 21
purpose: 운용 제약: QCC DMIC2 공유·전화통화 시나리오에서 빔포밍 가용성 토론 수렴
type: tasks
maturity: experimental
tags: [beamforming, agent-log, discuss]
---

# [단계2·토론 21] 운용 제약: QCC DMIC2 공유·전화통화 시나리오에서 빔포밍 가용성

## 찬
## 찬성 — 빔포밍과 전화통화 공존 가능하다

**논거 1: 0x36 classic_type 필드가 크래들/OTHER를 이미 구분하고 있어 HFP 판별 확장이 현실적이다.**
현재 `ble_communication.c:195`의 0x36 패킷은 `classic_state`(0=DISCONN, 1=CONN)와 `classic_type`(0=UNKNOWN, 1=CRADLE, 2=OTHER)을 이미 분리 수신한다. `classic_type` 값에 `3=PHONE_CALL(HFP SCO)`를 추가하면 신규 패킷 opcode 없이 기존 파싱 구조를 그대로 활용하여 전화통화 시작/종료를 통보할 수 있다. 코드 변경 범위가 최소화된다.

**논거 2: DIO17(DMIC2 데이터) fan-out 공유는 전기적 충돌 없이 E8300·QCC 동시 수신이 가능하다.**
DMIC2(U9) OUT2(DIO17)는 오픈-드레인이 아닌 수동 수신(passive receive)이므로 E8300과 QCC가 동시에 리드 리시버로 연결되어 있어도 버스 충돌이 발생하지 않는다(QCC 공유 마이크 제약 분석가). 즉 클럭 마스터 문제만 해결되면 데이터 라인 자체는 물리적 공존이 가능하다.

**논거 3: DIO10 클럭 마스터십 전환 비용이 낮다.**
E8300이 2-DMIC 구성에서 DIO10을 `DIO_MODE_ADCCLK`로 구동하고, 1-DMIC로 후퇴 시 `DIO_MODE_DISABLE`로 바꾸는 코드가 `lib_audio_in.c:154/116`에 이미 구현되어 있다. SPI 명령 수신 후 `disable_DMIC()` → `configure_audio_path_all(1-DMIC)`를 호출하는 인터럽트 핸들러 추가만으로 클럭 소유권 양도가 가능하다. 전환 지연은 SPI ISR 사이클 + 레지스터 write 1~2회 수준이다.

**논거 4: 10ms 대기는 보수적이지만 SPI 폴링 주기 내 처리 가능하다.**
현재 SPI는 QCC가 마스터인 DMA 완료 인터럽트 기반(`driver_SPI.c:132-172`)이다. QCC의 SPI 폴링 주기가 수ms 단위라면 명령 수신 → E8300 DMIC2 비활성화의 왕복 레이턴시는 10ms 이내 충분히 달성된다. HFP SCO 연결 자체가 수백ms 협상 과정을 거치므로 10ms 타이밍 여유는 충분하다.

## 반
## 반대 — 현 구조에서 충돌 없는 공존은 보장되지 않는다

**논거 1: QCC가 DIO10 클럭 마스터인지 슬레이브인지 미확인 — 역전 시 충돌 구조가 근본적으로 다르다.**
QCC 공유 마이크 제약 분석가가 명시한 미해결 사항: 전화통화 SCO 연결 시 QCC가 DMIC 클럭 마스터일 경우 E8300이 동시에 `DIO_MODE_ADCCLK`로 DIO10을 구동하면 양측 드라이버 충돌(output-to-output)이 발생한다. 이 경우 단순 DISABLE 전환으로는 해결 불가이며, E8300이 클럭을 완전히 내리기 전 QCC가 클럭 마스터로 전환하는 핸드오프 프로토콜이 필요하다. 회로도 미확인 상태에서는 이 위험을 배제할 수 없다.

**논거 2: 전화통화를 E8300이 감지할 수단이 현재 없다 — I2S_FLAG와 0x36 패킷 모두 불충분하다.**
`현재 상태 정리 및 복기.md:7`: "I2S_FLAG(DIO29) 만으로는 어떤 상황인지 알 수 없음." 0x36 패킷의 `classic_type`은 현재 CRADLE(1)/OTHER(2)만 정의되어 있고, 전화통화 HFP를 별도 값으로 QCC 측에서 구분하여 송신하는 구현이 존재하지 않는다(`ble_commonProtocol.h:45`, `ble_communication.c:199-204`). 프로토콜 협의 및 QCC 펌웨어 변경 없이는 E8300이 전화통화 진입을 알 방법이 없다.

**논거 3: 통보가 늦을 경우 빔포밍 DSP에 통화 음성이 섞여드는 오작동이 불가역적으로 발생한다.**
QCC가 SCO 연결 완료 후 DIO17로 수음을 시작하는 시점과 E8300이 SPI 명령을 수신하여 DMIC2를 비활성화하는 시점 사이에 반드시 갭이 존재한다. 이 갭 동안 E8300의 decimation ch2가 통화 마이크 신호를 캡처하여 delay-and-sum 빔포밍 입력으로 처리하면 빔 방향이 왜곡되고 통화 음성이 출력 오디오에 누출된다. 비가청 주파수 마커 등 하드웨어 동기화 수단이 없으므로 소프트웨어 타이밍 갭을 zero로 만들 수 없다.

**논거 4: 전화통화 종료 후 재활성화 시 FIFO stale 데이터 및 필터 flush 레이턴시가 있다.**
`disable_DMIC() → enable_DMIC()` 재진입 시 FA0_1 FIFO에 잔류한 stale 샘플이 CFX DSP에 전달될 가능성이 있으며(IOC/FIFO 경로 분석가 미해결), decimation WDF 필터 flush에 필요한 settling 사이클이 데이터시트에 명시되지 않아 재활성화 후 수십 샘플의 과도 구간이 발생할 수 있다. 빔포밍 지향 오류가 일시적으로 나타날 수 있다.

## 수렴 결론
## 수렴 결론

### 확정 사실 (데이터시트·코드 근거)

**이미지 확인으로 기하 방향 확정**: `착용 시 앞 방향 마이크 위치 정보.png` 분석 결과 Right ear front mic = U7/DMIC1(파란 화살표), Left ear front mic = U9/DMIC2(빨간 화살표)임이 명확하다. `현재 상태 정리 및 복기.md:18-24`의 "DMIC2[1] == DMIC1[0]"는 Left(DMIC2)가 앞 방향이므로 Left가 먼저 도달하고 Right(DMIC1/ch1)를 지연시켜야 함을 확인한다.

**지연 레지스터 설정 확정**: HW 고유 스큐(ch2가 ch1보다 1/8 샘플 선행)를 반영하면 ch1(Right/DMIC1)에 `DELAY_INTEGER=6, DELAY_FRACTIONAL=14`를 설정하면 총 0.8083 샘플(50.52µs) 보정이 달성된다. 목표 0.933 - 1/8(스큐 상쇄) = 0.808 샘플이며 오차 0.0243µs로 양자화 내 처리 가능하다(사실 D, HW p.450-451).

### 쟁점 판정

**공존 가능성: 조건부 가능, 현재 상태에서는 불가**

찬성 측의 핵심 주장(DIO17 fan-out 비충돌, 기존 DISABLE/ENABLE 코드 존재)은 타당하나, 반대 측의 논거 1·2가 더 우선한다.

- **클럭 충돌 위험**: QCC가 전화통화 시 DIO10 클럭 마스터인지 확인되지 않았다. 이것이 확인되지 않은 상태에서 E8300 2-DMIC 활성화는 보드 파손 가능성이 있는 output-to-output 충돌 리스크를 수반한다. 이 문제는 회로도 또는 QCC 펌웨어 확인으로만 해소된다.

- **프로토콜 갭**: 현재 0x33~0x36 패킷 범위에 DMIC2 점유 통보 명령이 존재하지 않는다(`ble_commonProtocol.h:42-45`). 0x36의 `classic_type` 확장이 가장 낮은 비용의 해결책이지만, QCC 펌웨어에서 HFP SCO 연결 시 해당 값을 전송하도록 수정해야 한다.

### 필요 조치 (우선순위 순)

**조치_1 (블로커)**: QCC 회로도 또는 QCC 펌웨어 담당자에게 전화통화 시 DIO10(DMIC CLK2)의 클럭 마스터 주체 확인. E8300이 마스터라면 단순 DISABLE 전환으로 충분. QCC가 마스터라면 클럭 핸드오프 시퀀스 설계 필요.

**조치_2 (프로토콜)**: 0x36 패킷 `classic_type` 필드에 `3=HFP_SCO_START`, `4=HFP_SCO_END` 값 추가 정의. `ble_commonProtocol.h`와 QCC 측 코드 동시 변경. E8300은 수신 즉시 `disable_DMIC()`(1-DMIC 후퇴) 또는 `enable_DMIC()`(2-DMIC 복귀)를 호출하는 핸들러를 `ble_communication.c:189-215` 분기에 추가.

**조치_3 (구현)**: FIFO flush를 위해 `enable_DMIC()` 재호출 후 최소 1 block_size(16샘플, 1ms@16kHz) 대기 또는 FA0_1 FIFO 명시적 flush 후 CFX 루프에 mic1 조건 복원(`main.c:182`의 주석 해제).

### 잔여 불확실성

온도 0→40°C 구간에서 음속 변화로 인한 빔 지연 오차 최대 4.12µs(15.8 FRAC LSB)는 고정 레지스터값 사용 시 허용 가능한지 응용 요구 사양 미정이다. 또한 QCC가 실제로 DIO17 물리 배선을 통해 DMIC2 데이터를 수신하는지 회로도 확인이 필요하다(코드 주석 "QCC 담당"만 존재).

빔포밍 DMIC2 공유 공존은 클럭 마스터 확인 + 프로토콜 0x36 확장의 두 단계 해결 후 구현 가능.

## 실행 인사이트


