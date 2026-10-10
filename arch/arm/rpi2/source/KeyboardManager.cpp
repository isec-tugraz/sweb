#include "KeyboardManager.h"
#include "board_constants.h"

KeyboardManager *KeyboardManager::instance_ = 0;

KeyboardManager::KeyboardManager() :
    keyboard_buffer_(256), extended_scancode(0), keyboard_status_(0), next_is_up_(0)
{
}

KeyboardManager::~KeyboardManager()
{
}

void KeyboardManager::kb_wait()
{
}

void KeyboardManager::send_cmd(uint8 cmd __attribute__((unused)), uint8 port __attribute__((unused)))
{
}

void KeyboardManager::serviceIRQ()
{
  volatile uint32* flags = (volatile uint32*)(SERIAL_BASE + SERIAL_FLAG_REGISTER);
  volatile uint32* data = (volatile uint32*)SERIAL_BASE;

  while (!(*flags & SERIAL_RECEIVE_EMPTY))
  {
    uint32 received = *data;
    if (received & 0xF00) // framing, parity, break or overrun error
      continue;

    uint8 key = received;
    if (key == '\r')
      key = '\n';
    else if (key == 127)
      key = '\b';

    if (key)
      keyboard_buffer_.put(key);
  }

  *(volatile uint32*)(SERIAL_BASE + SERIAL_INTERRUPT_CLEAR_REGISTER) = SERIAL_RECEIVE_INTERRUPTS;
}

void KeyboardManager::modifyKeyboardStatus(uint8 sc __attribute__((unused)))
{
}

void KeyboardManager::emptyKbdBuffer()
{
  keyboard_buffer_.clear();
}

void KeyboardManager::setLEDs()
{
}

uint32 KeyboardManager::convertScancode(uint8 scancode)
{
  return scancode;
}
