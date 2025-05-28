#include "debug_bochs.h"

#include "KeyboardManager.h"
#include "board_constants.h"

static bool pl011_uart_enabled = false;

void writeChar2Bochs( char char2Write )
{
  if (unlikely(!pl011_uart_enabled)) {
    // Enable BCM2835 PL011 UART
    *((volatile unsigned long*)(SERIAL_BASE + SERIAL_CONTROL_REGISTER)) |= 
      (1 << 0) | // UARTEN uart enable bit
      (1 << 8) | // TXE transmit enable bit
      (1 << 9);  // RXE receive enable bit

      pl011_uart_enabled = true;
  }

  /* Wait until the serial buffer is empty */
  while (*(volatile unsigned long*)(SERIAL_BASE + SERIAL_FLAG_REGISTER)
                                     & (SERIAL_BUFFER_FULL));
  /* Put our character, c, into the serial buffer */
  *(volatile unsigned long*)SERIAL_BASE = char2Write;
}

void writeLine2Bochs( const char * line2Write )
{
  const char *currentChar;

  uint8 counter = 0;

  for( currentChar = line2Write; (*currentChar != '\0') && (counter++ < 250); currentChar++ )
    writeChar2Bochs( *currentChar );
}
