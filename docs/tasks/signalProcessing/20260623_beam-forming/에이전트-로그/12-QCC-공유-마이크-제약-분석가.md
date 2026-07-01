---
name: 분석 페르소나 12
purpose: QCC 공유 마이크 제약 분석가 분석 결과
type: tasks
maturity: experimental
tags: [beamforming, agent-log, analysis]
---

# [단계1·분석 12] QCC 공유 마이크 제약 분석가

**신뢰도**: medium

## 결론
DMIC2(U9)는 전화통화 시 QCC가 HFP SCO용 마이크로 사용하므로 E8300 상시 빔포밍과 클럭 소유권 및 타이밍 충돌이 발생한다. 충돌 해결을 위해 현재 SPI 프로토콜에 존재하지 않는 "QCC DMIC2 점유 시작/종료" 통보 명령을 신규 정의하고, 명령 수신 후 E8300이 DIO10/DIO17을 비활성화하는 시퀀스를 구현해야 한다. 10ms 대기는 SPI 패킷 처리 사이클 최악값 추정치로 현재 코드에 구체적 수치 근거 없음.

## 발견(근거)
- **DMIC2(U9) 클럭 라인(DIO10)은 2-DMIC 구성 시 E8300이 ADCCLK 출력 마스터가 된다. QCC는 이 클럭에 종속되므로, E8300이 DIO10을 DISABLE하면 QCC 전화통화 마이크 입력이 차단된다.**
  - 근거: lib_audio_in.c:154 — 2-DMIC 구성 enable_DMIC()에서 `Sys_DIO_Config(DIO10, DIO_MODE_ADCCLK)`. 1-DMIC 구성에서는 DIO10이 `DIO_MODE_DISABLE`(lib_audio_in.c:116). 주석 lib_audio_in.c:119: '클럭 설정은 하나만 가능(DMIC_CLK1/CAL 기본)'. (lib_audio_in.c:116, 119, 154)
- **DMIC2 데이터 라인(DIO17)은 전기적으로 E8300과 QCC가 동시에 읽기 가능(fan-out 충돌 없음)이나, 클럭 동기화 주체가 E8300이므로 E8300이 빔포밍을 중단하면 QCC의 DMIC2 샘플링도 중단된다.**
  - 근거: lib_audio_in.c:139(2-DMIC 구성): `DIO->SRC_DMIC_DATA = DMIC1_DATA_SRC_DIO_23 | DMIC2_DATA_SRC_DIO_17`. DMIC2 데이터(DIO17)는 마이크 출력이므로 수동 수신(open-drain 아님) — QCC와 E8300 양쪽이 독립 수신 가능. 단 클럭(DIO10)이 ADCCLK 출력이어야 마이크가 동작. (lib_audio_in.c:139)
- **I2S_FLAG(DIO29) 단독으로는 전화통화 여부를 판별할 수 없다. QCC가 I2S를 사용하는 3가지 시나리오(핸드폰 BT 미디어, 크래들 BT, 전화통화) 중 전화통화 시에만 DMIC2가 QCC에게 필요하지만 현재 구별 수단이 없다.**
  - 근거: 현재 상태 정리 및 복기.md:1-9: 'I2S_FLAG(DIO29)가 1이면 QCC로부터 I2S 신호 입력되는 상태. QCC가 E8300으로 I2S 신호를 입력하는 경우는 3가지 상황 [미디어/크래들/전화통화]. 문제는 E8300 입장에서 I2S_FLAG 만으로는 어떤 상황인지 알 수 없음.' (현재 상태 정리 및 복기.md:1-9)
- **현재 SPI 프로토콜에 'QCC가 DMIC2 전화통화용 점유 시작/종료'를 E8300에 통보하는 명령이 존재하지 않는다. 이 명령을 신규 정의해야 상시 빔포밍과 전화통화 공존이 가능하다.**
  - 근거: ble_communication.c에서 확인된 QCC 관련 패킷: 0x33(Battery), 0x34(Power info), 0x35(LED Indication), 0x36(클래식 상태). DMIC2 점유 상태 통보 패킷은 코드 전체에서 발견되지 않음. 현재 상태 정리 및 복기.md:8: '문제 해결을 위해 QCC에서 전화 통화를 위해 DMIC2를 사용한다는 SPI 프로토콜이 필요함'. (ble_communication.c:90-188, 현재 상태 정리 및 복기.md:8)
- **SPI 명령 처리 후 E8300이 DMIC2를 비활성화하기까지의 대기 시간이 10ms 이내로 예상되나, 이 수치는 코드에 명시적 근거가 없는 추정값이다.**
  - 근거: 현재 상태 정리 및 복기.md:9: 'SPI 프로토콜 명령 처리에 대한 대기 시간도 필요함. (10msec 이내로 예상)'. 코드에서 SPI 트랜잭션 주기 명시 없음 — driver_SPI.c는 DMA 완료 인터럽트 기반이며 주기는 QCC(마스터)의 폴링 간격에 의존. (현재 상태 정리 및 복기.md:9, driver_SPI.c:132-172)
- **상시 빔포밍 구성(2-DMIC)에서 전화통화 전환 시 충돌 시퀀스: QCC가 SCO 연결 후 DMIC2 사용 시작 → E8300은 이를 모름 → E8300 decimation ch2가 DMIC2 데이터를 계속 캡처하면서 QCC도 동일 데이터 수신 → 클럭 공유로 동작은 되나, E8300 측 빔포밍 입력에 통화 음성이 섞여드는 부작용 발생.**
  - 근거: 사실 E(HW §18.2 p.563): DMIC 입력은 6th order CIC pre-decimation 필터. DIO17은 마이크 출력으로 fan-out 가능. lib_audio_in.c:154에서 2-DMIC 시 DIO10이 ADCCLK 출력이므로 클럭은 E8300이 마스터. 두 장치가 동시에 수신 가능하나 빔포밍 DSP가 통화 마이크 신호를 환경음으로 처리하는 오작동 유발. (사실 E (HW §18.2 p.563), lib_audio_in.c:154)

## 미해결 질문
- QCC가 전화통화 SCO 연결 시 실제로 DIO10(CLK2)과 DIO17(OUT2)을 어떻게 사용하는지 QCC 펌웨어/데이터시트 확인 필요 — QCC가 클럭 마스터인지 슬레이브인지 미확인. 클럭 마스터가 QCC라면 충돌 구조가 역전됨.
- SPI 프로토콜 트랜잭션 주기(QCC가 SPI 마스터로서 E8300을 폴링하는 간격)가 얼마인지 미확인 — 10ms 대기 추정의 실제 근거가 됨. QCC 측 SPI 마스터 코드 또는 프로토콜 문서 필요.
- 전화통화 종료 후 E8300이 DMIC2를 재활성화하는 타이밍 조율이 필요한지 — QCC가 종료 통보 명령을 보내고 E8300이 enable_DMIC()를 재호출하는 왕복 시퀀스 설계가 필요한가.
- DIO17(DMIC_OUT2) fan-out이 보드상에서 실제로 E8300과 QCC 양쪽에 물리적으로 연결되어 있는지 회로도 확인 필요 — 현재 그라운딩 사실에서 DIO17이 QCC에도 직접 연결된다는 명시적 증거가 없음(QCC 담당이라는 코드 주석만 존재).
- 현재 1-DMIC 구성에서 mic1 인터럽트 플래그가 주석처리되어 있음(cfx main.c:182). 2-DMIC 빔포밍 전환 시 mic1 플래그 처리 복원 및 delay-and-sum 로직을 CFX DSP에 추가해야 하며, 이 부분의 처리 사이클 예산이 충분한지 미확인.
