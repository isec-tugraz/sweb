#pragma once

#include "types.h"

typedef struct
{
  size_t q1;
  size_t q2;
} NeonQ;

typedef struct ArchThreadRegisters
{
  size_t X[31];
  NeonQ Q[32];
  size_t SPSR;
  size_t ELR;
  size_t SP;
  size_t TTBR0;
  size_t SP_SM;
} ArchThreadRegisters;
