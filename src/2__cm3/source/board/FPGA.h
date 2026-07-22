
#ifndef FPGA_H__
#define FPGA_H__

#include <processorDirective.h>

#if defined(FPGA_Ver_is_100)
#include <FPGA_ver1_0_0.h>
#elif defined(FPGA_Ver_is_110)
#include <FPGA_ver1_1_0.h>
#elif defined(FPGA_Ver_is_200)
#include <FPGA_ver2_0_0.h>
#elif defined(FPGA_Ver_is_210)
#include <FPGA_ver2_1_0.h>
#elif defined(FPGA_Ver_is_230)
#include <FPGA_ver2_1_0.h>
#elif defined(FPGA_Ver_is_240)
#include <FPGA_ver2_4_0.h>
#elif defined(FPGA_Ver_is_270)
#include <FPGA_ver2_7_0.h>
#elif defined(FPGA_Ver_is_280)
#include <FPGA_ver2_8_0.h>
#else
#error FPGA is NOT selected.
#endif

#endif
