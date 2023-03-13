#pragma once

#include "types.h"

struct
{
  uint64 read                      :1;
  uint64 write                     :1;
  uint64 execute                   :1;
  uint64 reserved_1                :5;
  uint64 accessed                  :1;
  uint64 ignored_1                 :1;
  uint64 execute_user_mode         :1;
  uint64 ignored_2                 :1;
  uint64 page_ppn                  :32;
  uint64 page_ppn_hi               :4;
  uint64 reserved_2                :4;
  uint64 ignored_3                 :12;
} __attribute__((__packed__)) EPTPageMapLevel4Entry;

static_assert(sizeof(EPTPageMapLevel4Entry) == 8, "EPTPageMapLevel4Entry is not 64 bit");

struct
{
  uint64 read                      :1;
  uint64 write                     :1;
  uint64 execute                   :1;
  uint64 reserved_1                :5;
  uint64 accessed                  :1;
  uint64 ignored_1                 :1;
  uint64 execute_user_mode         :1;
  uint64 ignored_2                 :1;
  uint64 page_ppn                  :32;
  uint64 page_ppn_hi               :4;
  uint64 reserved_2                :4;
  uint64 ignored_3                 :12;
} __attribute__((__packed__)) EPTPageDirectoryPointerTableEntry;

static_assert(sizeof(EPTPageDirectoryPointerTableEntry) == 8, "EPTPageDirectoryPointerTableEntry is not 64 bit");

struct
{
  uint64 read                      :1;
  uint64 write                     :1;
  uint64 execute                   :1;
  uint64 reserved_1                :5;
  uint64 accessed                  :1;
  uint64 ignored_1                 :1;
  uint64 execute_user_mode         :1;
  uint64 ignored_2                 :1;
  uint64 page_ppn                  :32;
  uint64 page_ppn_hi               :4;
  uint64 reserved_2                :4;
  uint64 ignored_3                 :12;
} __attribute__((__packed__)) EPTPageDirectoryEntry;

static_assert(sizeof(EPTPageDirectoryEntry) == 8, "EPTPageDirectoryEntry is not 64 bit");

struct
{
  uint64 read                      :1;
  uint64 write                     :1;
  uint64 execute                   :1;
  uint64 ept_memory_type           :3;
  uint64 ignore_pat                :1;
  uint64 ignored_1                 :1;
  uint64 accessed                  :1;
  uint64 dirty                     :1;
  uint64 execute_user_mode         :1;
  uint64 ignored_2                 :1;
  uint64 page_ppn                  :32;
  uint64 page_ppn_hi               :4;
  uint64 reserved                  :4;
  uint64 ignored_3                 :11;
  uint64 suppress_ve               :1;
} __attribute__((__packed__)) EPTPageTableEntry;

static_assert(sizeof(EPTPageTableEntry) == 8, "EPTPageTableEntry is not 64 bit");

struct 
{
  uint64 memory_type               :3; // bit 2:0 (0 = Uncacheable (UC) - 6 = Write - back(WB))
  uint64 page_walk_length          :3; // bit 5:3 (This value is 1 less than the EPT page-walk length) 
  uint64 dirty_access_enabled      :1; // bit 6  (Setting this control to 1 enables accessed and dirty flags for EPT)
  uint64 reserved_1                :5; // bit 11:7 
  uint64 pml4                      :32;
  uint64 pml4_hi                   :4;
  uint64 reserved_2                :16;
} __attribute__((__packed__)) EPTP;

static_assert(sizeof(EPTP) == 8, "EPTP is not 64 bit");
