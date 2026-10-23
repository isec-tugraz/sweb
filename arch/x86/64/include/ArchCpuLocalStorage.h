#pragma once

#include "types.h"
#include "cpu_local.h"

namespace CPULocalStorage
{
  void initialiseBootCPU();
  size_t size();
  void initialise(void* storage);
}
