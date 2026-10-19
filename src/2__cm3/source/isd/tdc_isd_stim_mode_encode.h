#ifndef TDC_ISD_STIM_MODE_ENCODE_H
#define TDC_ISD_STIM_MODE_ENCODE_H

#include <tdc_shm.h>  // EN___STIMULATION_MODE

/*
  자극 모드를 내부기 설정 레지스터 비트로 바꾼다.

  이 레지스터의 하위 4비트는 두 필드로 나뉜다.

    비트 [3:2]  MP_CONFIG  기준전극 모드  <- tdc_isd_stim_mode_reference_bits()
    비트 [1:0]  STIM_MODE  자극 출력 모드 <- tdc_isd_stim_mode_output_bits()

  2026-08-06 이전에는 같은 변환이 3파일 5쌍(스위치 10블록)에 복제돼 있었다.
    isd/tdc_isd_map_ecap.c          2쌍
    isd/tdc_isd_map_specific_stim.c 1쌍
    isd/tdc_isd_stim_para_setting.c 2쌍

  시프트는 호출부에 남긴다. 레지스터를 한 비트씩 쌓아 올리는 흐름이 그대로 보여야
  어느 필드가 어디에 놓이는지 읽히기 때문이다. 이 함수들은 값만 돌려준다.

      w_isd_registerValue = w_isd_registerValue << 2;
      w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_reference_bits(mode);
*/

// 비트 [3:2] 기준전극 모드.
// 모노폴라 세 모드는 열거형 값(1~3)을 그대로 쓰고, 그 밖은 0(en__referenceNA)이다.
// 바이폴라 · 공통접지 · 동시자극은 기준전극을 쓰지 않으므로 접지를 끊는다.
// 이 필드의 하드웨어 리셋값이 3(en__monopolr_BothRodBody)이라, 끊으려면 0을 명시해야 한다.
int tdc_isd_stim_mode_reference_bits(EN___STIMULATION_MODE mode);

// 비트 [1:0] 자극 출력 모드.
// 모노폴라 0 · 바이폴라 1 · 공통접지 2 · 동시자극 3.
int tdc_isd_stim_mode_output_bits(EN___STIMULATION_MODE mode);

#endif
