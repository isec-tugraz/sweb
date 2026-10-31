#include "ArchCpuLocalStorage.h"

extern uint8 tls_start[], tls_data_end[], tls_end[];
static uint8 boot_cpu_tls[32768] __attribute__((aligned(16)));

size_t CPULocalStorage::size()
{
  return tls_end - tls_start + sizeof(void*);
}

void CPULocalStorage::initialise(void* storage)
{
  size_t length = tls_end - tls_start;
  // memcpy/memset check currentThread's stack canary, so cannot bootstrap TLS.
  uint8* bytes = (uint8*)storage;
  size_t data_length = tls_data_end - tls_start;
  for (size_t i = 0; i < length; ++i)
    bytes[i] = i < data_length ? tls_start[i] : 0;
  void** base = (void**)((uint8*)storage + length);
  *base = base; // x86 TLS uses negative offsets from FS:0, which points to itself.
  size_t address = (size_t)base;
  // FS is reserved for kernel CPU-local storage; userspace TLS is not implemented.
  asm volatile("wrmsr" : : "c"(0xC0000100), "a"((uint32)address), "d"((uint32)(address >> 32)) : "memory");
}

void CPULocalStorage::initialiseBootCPU()
{
  initialise(boot_cpu_tls);
}
