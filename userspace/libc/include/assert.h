#pragma once

#include <stdio.h>
#include <stdlib.h>

#define assert(X) do { if (!(X)) {\
    printf("Assertion failed: '%s', file:line %s:%d, function %s\n", #X, __FILE__, __LINE__, __FUNCTION__); \
 exit(-1); } } while (0)

