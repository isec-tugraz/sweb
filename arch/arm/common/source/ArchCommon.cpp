#include "ArchCommon.h"
#include "ArchBoardSpecific.h"
#include "offsets.h"
#include "kprintf.h"
#include "ArchMemory.h"
#include "TextConsole.h"
#include "FrameBufferConsole.h"
#include "backtrace.h"
#include "SWEBDebugInfo.h"

#define PHYSICAL_MEMORY_AVAILABLE 8*1024*1024

extern void* kernel_end_address;

pointer ArchCommon::getKernelEndAddress()
{
   return (pointer)&kernel_end_address;
}

pointer ArchCommon::getFreeKernelMemoryStart()
{
   return (pointer)&kernel_end_address;
}

pointer ArchCommon::getFreeKernelMemoryEnd()
{
   return (pointer)&kernel_end_address;
}


uint32 ArchCommon::haveVESAConsole(uint32 is_paging_set_up __attribute__((unused)))
{
  return true;
}

uint32 ArchCommon::getNumModules(uint32 is_paging_set_up __attribute__((unused)))
{
  return 0;
}

uint32 ArchCommon::getModuleStartAddress(uint32 num __attribute__((unused)))
{
  assert(false);
}

uint32 ArchCommon::getModuleEndAddress(uint32 num __attribute__((unused)))
{
  assert(false);
}

uint32 ArchCommon::getVESAConsoleHeight()
{
  return 480;
}

uint32 ArchCommon::getVESAConsoleWidth()
{
  return 640;
}

pointer ArchCommon::getVESAConsoleLFBPtr(uint32 is_paging_set_up __attribute__((unused)))
{
  return ArchBoardSpecific::getVESAConsoleLFBPtr();
}

pointer ArchCommon::getFBPtr(uint32 is_paging_set_up __attribute__((unused)))
{
  return getVESAConsoleLFBPtr();
}

uint32 ArchCommon::getVESAConsoleBitsPerPixel()
{
  return 16;
}

uint32 ArchCommon::getNumUseableMemoryRegions()
{
  return 1;
}

uint32 ArchCommon::getUsableMemoryRegion(uint32 region, pointer &start_address, pointer &end_address, uint32 &type)
{
  return ArchBoardSpecific::getUsableMemoryRegion(region, start_address, end_address, type);
}

Console* ArchCommon::createConsole(uint32 count)
{
  ArchBoardSpecific::frameBufferInit();
  return new FrameBufferConsole(count);
}

Stabs2DebugInfo const *kernel_debug_info = 0;

void ArchCommon::initDebug()
{
  extern unsigned char swebdbg_start_address_nr;
  extern unsigned char swebdbg_end_address_nr;

  kernel_debug_info = new SWEBDebugInfo((const char *)&swebdbg_start_address_nr, (const char*)&swebdbg_end_address_nr);
}

extern "C" void halt()
{
  asm("mcr p15, 0, %[v], c7, c0, 4" : : [v]"r" (0)); // Wait for interrupt
}

void ArchCommon::idle()
{
  ArchBoardSpecific::onIdle();
  halt();
}


extern "C" void __aeabi_atexit()
{
  assert(false && "would not make sense in a kernel");
}

extern "C" void __aeabi_unwind_cpp_pr0()
{
  assert(false && "no exception handling implemented");
}

extern "C" void raise()
{
  assert(false && "no exception handling implemented");
}
