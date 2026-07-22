#ifndef internalStimulationChip_H__
#define internalStimulationChip_H__

#include <processorDirective.h>

/* 지원 버전: ISD_Ver_is_112 만. 구세대(1.0.0/1.1.0/1.1.1)는
 * Sullivan 1~1.5 세대 레거시로 제거했다(2026-07-22). 필요 시 git 이력에서 복원. */
#if defined(ISD_Ver_is_112)
#include <isd_ver1_1_2.h>
#else
#error ISD version is NOT selected or NOT supported. (supported: ISD_Ver_is_112)
#endif
#endif
