#ifndef __tdc_isd_pcm_h__
#define __tdc_isd_pcm_h__

#include "processorDirective.h" //ok

// PCM 모드
#define PcmBitStream_Mode_FillZero        0
#define PcmBitStream_Mode_Preamble        1
#define PcmBitStream_Mode_LiveStimulation 2
#define PcmBitStream_Mode_SepcificCommand 3
#define PcmBitStream_Mode_NopStandby      4
#define PcmBitStream_Mode_NopBacktel      5
#define PcmBitStream_Mode_notApplicable   99

// PCM Mold

#define pcm_Mold_Preamble             0xFF555 // 1이 연속되다가(8bit) 0과1일 반복(12bit)
#define pcm_Mold_NopStandby           0x25555 // 0010-0101010101010101(16bit)
#define pcm_Mold_NopBacktel           0x35555 // 0011-0101010101010101(16bit)
#define pcm_Mold_PulsePhaseWidth      0x60000 // 011-0-0000-0000-펄스폭(8bit)
#define pcm_Mold_BacktelConfiguration 0x60100 // 011-0-00000001(addr_PCM register_01)-xxxxxxxx(register Value) 	// 011-0-0000-0001-link모드(1=무선, 0=유선)-BackterCalibration(5bit, 유선=01010, 무선=01100이 기본값)-BacktelMode( 0=8bit register, 1=12bit adc) - BackterOnOff( 0=off, 1=0n)
#define pcm_Mold_Stimulation          0x40000 // 0100-선행펄스모양(1bit)-전극번호(5bit)-자극크기(8bit)-00(2bit)
#define pcm_Mold_configuration        0x50000 // 0101-레지스터 주소(7bit)-w/r(1bit)-data(8bit)

// ISD 연결 확인용 출력 임의 값... 레지스터는 읽기 가능한 레지스터 중 path check 레지스터를 선택하였다.. 확인 후 변경 가능

#define pcm_Mold_connectionCheck_ForwardPath  0x50600 // 0101-0000011(주소-7bit)-0 (w/r-1bit)-00000000(data-8bit)
#define pcm_Mold_connectionCheck_ReadISDPower 0x50400 // 0101-0000010(주소-7bit)-0 (w/r-1bit)-00000000(data-8bit)

#define firstPulsePhasePositionAtPCM_Mold 15
#define electrodIndexPositionAtPCM_Mold   10
#define stimulationPositionAtPCM_Mold     2

#define BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation 0
#define BackelCircuitEnabled_duringLiveStimulation                  1
#define BackelCircuitDisabled_readPcmFired_duringLiveStimulation    2



#endif
