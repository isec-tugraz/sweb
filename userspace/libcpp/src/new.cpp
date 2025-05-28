#include "stddef.h"

void *operator new(std::size_t size)
{
  return nullptr;
}

void operator delete(void *ptr)
{
  return;
}

void *operator new[](std::size_t size)
{
  return nullptr;
}

void operator delete[](void *ptr)
{
  return;
}
