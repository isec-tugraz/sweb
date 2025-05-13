#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CMAKE_X86_64) || defined(CMAKE_ARMV8_RPI3)
typedef long unsigned int size_t;
typedef long          int ssize_t;
#else
typedef unsigned int size_t;
typedef          int ssize_t;
#endif

typedef size_t  off_t;
typedef size_t  mode_t;
typedef size_t  uid_t;
typedef size_t  gid_t;
typedef ssize_t pid_t;

#ifdef __cplusplus
}
#endif
