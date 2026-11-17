#pragma once

#include "types.h"

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * kernel panic function writes the message and the stack trace to the screen and
 * dies after that
 *
 */
void kpanict(const char* message);

#ifdef __cplusplus
}
#endif
