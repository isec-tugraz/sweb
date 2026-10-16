#include "nonstd.h"
#include "sys/syscall.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "stdlib.h"

int createprocess(const char* path, int sleep)
{
  return __syscall(sc_createprocess, (long) path, sleep, 0x00, 0x00, 0x00);
}

extern int main();

typedef void (*constructor_t)(void);

extern constructor_t __preinit_array_start[];
extern constructor_t __preinit_array_end[];
extern constructor_t __init_array_start[];
extern constructor_t __init_array_end[];

static void call_constructors(constructor_t* begin, constructor_t* end)
{
  while (begin != end)
    (*begin++)();
}

void _start()
{
  call_constructors(__preinit_array_start, __preinit_array_end);
  call_constructors(__init_array_start, __init_array_end);
  exit(main());
}
