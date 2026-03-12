/**
 * @file ci_util.h
 */

#ifndef __ci_util_h__
#define __ci_util_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <board.h>
#include <definitionsForAlgorithm.h>

void ci_util_indicate_critical_error(void);
void ci_util_assert(int a);

#endif  // __ci_util_h__
