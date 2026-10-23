#pragma once

// GCC TLS describes per-CPU storage once each CPU has its own TLS base.
#if defined(CMAKE_X86_64) && !defined(EXE2MINIXFS)
#define __cpu thread_local
#else
#define __cpu
#endif
