#pragma once

// https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf

// BCM2835 PL011 UART
#define SERIAL_BASE 0x86001000
#define SERIAL_FLAG_REGISTER 0x18
#define SERIAL_CONTROL_REGISTER 0x30
#define SERIAL_BUFFER_FULL (1 << 5)

#define PIC_BASE 0x9000B200
#define HCD_DESIGNWARE_BASE ((void*)0x90980000)
#define BOARD_LOAD_BASE 0

