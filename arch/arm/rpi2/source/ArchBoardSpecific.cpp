#include "ArchBoardSpecific.h"

#include "KeyboardManager.h"
#include "board_constants.h"
#include "InterruptUtils.h"
#include "ArchCommon.h"
#include "assert.h"
#include "offsets.h"
#include "ArchInterrupts.h"
#include "Scheduler.h"
#include "FrameBufferConsole.h"
#include "kprintf.h"

#define PHYSICAL_MEMORY_AVAILABLE 8*1024*1024

struct RPiFrameBufferStructure
{
  uint32 width;
  uint32 height;
  uint32 vwidth;
  uint32 vheight;
  uint32 pitch;
  uint32 depth;
  uint32 xoffset;
  uint32 yoffset;
  uint32 pointer;
  uint32 size;
  uint16 cmap[256];
};
struct RPiFrameBufferStructure fbs __attribute__ ((aligned (0x20)));
pointer framebuffer;

pointer ArchBoardSpecific::getVESAConsoleLFBPtr()
{
  return framebuffer;
}

uint32 ArchBoardSpecific::getUsableMemoryRegion(uint32 region __attribute__((unused)), pointer &start_address, pointer &end_address, uint32 &type)
{
  start_address = 0;
  end_address = ((PHYSICAL_MEMORY_AVAILABLE - ArchCommon::getVESAConsoleWidth() * ArchCommon::getVESAConsoleHeight() * ArchCommon::getVESAConsoleBitsPerPixel() / 8) & ~0xFFF);
  type = 1;
  return 0;
}

extern "C" void memory_barrier();

void ArchBoardSpecific::frameBufferInit()
{
  // frame buffer initialization from http://elinux.org/RPi_Framebuffer#Notes
  for (uint32 i = 0; i < 10 && (fbs.pointer == 0 || fbs.size == 0); ++i)
  {
    fbs.width = 640;
    fbs.height = 480;
    fbs.vwidth = fbs.width;
    fbs.vheight = fbs.height;
    fbs.pitch = 0;
    fbs.depth = 16;
    fbs.xoffset = 0;
    fbs.yoffset = 0;
    fbs.pointer = 0;
    fbs.size = 0;
    uint32* MAIL0_READ = (uint32*)0x9000b880;
    uint32* MAIL0_WRITE = (uint32*)0x9000b8A0;
    uint32* MAIL0_STATUS = (uint32*)0x9000b898;
    memory_barrier();
    while (*MAIL0_STATUS & (1 << 31));
    assert((((uint32)&fbs) & 0xF) == 0);
    *MAIL0_WRITE = VIRTUAL_TO_PHYSICAL_BOOT(((uint32)&fbs) & ~0xF) | (0x1);
    memory_barrier();
    uint32 read = 0;
    while ((read & 0xF) != 1)
    {
      while (*MAIL0_STATUS & (1 << 30));
      read = *MAIL0_READ;
    }
    memory_barrier();
    for (uint32 i = 0; i < 0x10000; ++i);
  }
  assert(fbs.pointer != 0);
  assert(fbs.width == fbs.vwidth);
  assert(fbs.height == fbs.vheight);
  assert(fbs.size == (fbs.width * fbs.height * fbs.depth / 8));
  framebuffer = fbs.pointer | IDENT_MAPPING_START;
}

void ArchBoardSpecific::onIdle()
{
}

void ArchBoardSpecific::enableTimer()
{
  *(volatile uint32*)(LOCAL_PIC_BASE + 0x0C) = 0; // route GPU interrupts to core 0
  // The system timer runs at 1 MHz. Compare 0 and 2 are reserved for the GPU.
  *(volatile uint32*)SYSTEM_TIMER_BASE = 1 << 1;
  *(volatile uint32*)(SYSTEM_TIMER_BASE + 0x10) = *(volatile uint32*)(SYSTEM_TIMER_BASE + 0x04) + 50000; // 20 Hz
  *(volatile uint32*)(PIC_BASE + 0x10) = 1 << 1; // system timer compare 1
}

void ArchBoardSpecific::setTimerFrequency(uint32 freq)
{
  (void)freq;
  debug(A_BOOT, "Sorry, setTimerFrequency not implemented!\n");
}

void ArchBoardSpecific::disableTimer()
{
  *(volatile uint32*)(PIC_BASE + 0x1C) = 1 << 1;
  *(volatile uint32*)SYSTEM_TIMER_BASE = 1 << 1;
}

void ArchBoardSpecific::enableKBD()
{
  *(volatile uint32*)(LOCAL_PIC_BASE + 0x0C) = 0; // route GPU interrupts to core 0
  *(volatile uint32*)(SERIAL_BASE + SERIAL_CONTROL_REGISTER) |= (1 << 0) | (1 << 8) | (1 << 9);
  *(volatile uint32*)(SERIAL_BASE + SERIAL_INTERRUPT_MASK_REGISTER) |= SERIAL_RECEIVE_INTERRUPTS;
  *(volatile uint32*)(PIC_BASE + 0x14) = 1 << 25; // UART IRQ 57
}

void ArchBoardSpecific::disableKBD()
{
  *(volatile uint32*)(PIC_BASE + 0x20) = 1 << 25;
  *(volatile uint32*)(SERIAL_BASE + SERIAL_INTERRUPT_MASK_REGISTER) &= ~SERIAL_RECEIVE_INTERRUPTS;
}

void ArchBoardSpecific::keyboard_irq_handler()
{
  KeyboardManager::instance()->serviceIRQ();
}

extern void timer_irq_handler();

void ArchBoardSpecific::timer0_irq_handler()
{
  if (*(volatile uint32*)SYSTEM_TIMER_BASE & (1 << 1))
  {
    assert(!ArchInterrupts::testIFSet());
    *(volatile uint32*)SYSTEM_TIMER_BASE = 1 << 1;
    *(volatile uint32*)(SYSTEM_TIMER_BASE + 0x10) = *(volatile uint32*)(SYSTEM_TIMER_BASE + 0x04) + 50000;

    timer_irq_handler();
  }
}

void ArchBoardSpecific::irq_handler()
{
  if (*(volatile uint32*)(PIC_BASE + 0x08) & (1 << 25))
    keyboard_irq_handler();

  if (*(volatile uint32*)(PIC_BASE + 0x04) & (1 << 1))
    timer0_irq_handler();
}

extern "C" void ArchBoardSpecific::disableMulticore(uint32* spin) {
  asm("mov r1, #0x1\n"
          "multicore_spin:\n"
          "ldrex r0, [%[r]]\n" // load the sync value
          "cmp r0, #0\n" // check whether this is the first core
          "strexeq r0, r1, [%[r]]\n"  // if yes, claim the sync value
          "cmpeq r0, #0\n" // did this succeed?
          "bne multicore_spin\n" // no - we are not the first core, spin indefinitely
  : : [r]"r"(spin));
}
