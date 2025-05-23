#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEXITED 4

extern pid_t waitpid(pid_t pid, int *status, int options);

#ifdef __cplusplus
}
#endif
