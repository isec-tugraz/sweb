#include "GlobalConstructors.h"

typedef void (*Constructor)();

extern "C" Constructor __preinit_array_start[];
extern "C" Constructor __preinit_array_end[];
extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];

static void callConstructors(Constructor* begin, Constructor* end)
{
  while (begin != end)
    (*begin++)();
}

void callGlobalConstructors()
{
  callConstructors(__preinit_array_start, __preinit_array_end);
  callConstructors(__init_array_start, __init_array_end);
}
