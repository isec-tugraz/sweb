#pragma once

// GCC ignores the naked attribute on AArch64. Branching to this computed
// target skips its prologue; the function must not return through an epilogue.
#define PSEUDO_NAKED_BODY(function)                              \
  asm volatile(                                                 \
      #function "_body:\n"                                     \
      ".set " #function "_body_offset, "                       \
          #function "_body - " #function "\n"                  \
      ".set " #function "_pseudo_naked, "                      \
          #function " + " #function "_body_offset\n")

#define PSEUDO_NAKED_TARGET(function) #function "_pseudo_naked"
