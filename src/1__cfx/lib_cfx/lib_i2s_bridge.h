/**
 * @file lib_i2s_bridge.h
 */

#ifndef __lib_i2s_bridge_h__
#define __lib_i2s_bridge_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

void build_bridge_C1_Q816_cfx(const int D[16], const int E[16], int S1[16], int S2[16]);

#endif // __lib_i2s_bridge_h__


