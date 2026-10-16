#include "stdio.h"

static int initialized;

class GlobalObject
{
  public:
    GlobalObject()
    {
      initialized = 1;
    }

    ~GlobalObject()
    {
      printf("C++ global destructor called.\n");
    }
};

GlobalObject global_object;

int main()
{
  if (!initialized)
    return 1;

  printf("C++ global constructor called.\n");
  return 0;
}
