
#ifndef FPGA_H__
#define FPGA_H__

#include <processorDirective.h>

/* 지원 버전: FPGA_Ver_is_270 만. 구세대(1.0.0~2.4.0)와 2.8.0 은
 * Sullivan 1~1.5 세대 레거시로 제거했다(2026-07-22). 필요 시 git 이력에서 복원. */
#if defined(FPGA_Ver_is_270)
#include <FPGA_ver2_7_0.h>
#else
#error FPGA version is NOT selected or NOT supported. (supported: FPGA_Ver_is_270)
#endif

#endif
