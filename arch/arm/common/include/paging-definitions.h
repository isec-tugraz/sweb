#pragma once

#include "types.h"


#define PAGE_DIR_ENTRIES 4096
#define PAGE_TABLE_ENTRIES 256
#define PAGE_SIZE 4096
#define PAGE_INDEX_OFFSET_BITS 12


// Constants for page fault handling
#define ERROR_MASK            0x00000007
#define PAGE_FAULT_USER       0x00000004
#define PAGE_FAULT_WRITEABLE  0x00000002
#define PAGE_FAULT_PRESENT    0x00000001

struct PageDirPageEntry
{
  size_t size                      :2;  // 1:0    | 2
  size_t bufferable                :1;  // 2      | 0
  size_t cachable                  :1;  // 3      | 0
  size_t reserved_3                :1;  // 4      | 0
  size_t domain                    :4;  // 8:5    | 0
  size_t reserved_2                :1;  // 9      | 0
  size_t permissions               :2;  // 11:10  | 1
  size_t reserved_1                :8;  // 19:12  | 0
  size_t page_ppn                  :12; // 31:20
} __attribute__((__packed__));

static_assert(sizeof(PageDirPageEntry) == 4, "PageDirPageEntry is not 32 bit");

struct PageDirPageTableEntry
{
  size_t size                      :2;  // 1:0    | 1
  size_t bufferable                :1;  // 2      | 0
  size_t cachable                  :1;  // 3      | 0
  size_t reserved_3                :1;  // 4      | 0
  size_t domain                    :4;  // 8:5    | 0
  size_t reserved_2                :1;  // 9      | 0
  size_t offset                    :2;  // 11:10  | 0 // 4 page tables would fit on 1 page
  size_t pt_ppn                    :20; // 31:10
} __attribute__((__packed__));

static_assert(sizeof(PageDirPageTableEntry) == 4, "PageDirPageTableEntry is not 32 bit");

typedef union
{
  struct PageDirPageTableEntry pt;
  struct PageDirPageEntry page;
} __attribute__((__packed__)) PageDirEntry;

typedef struct
{
  size_t size                      :2;  // 1:0    | 2
  size_t bufferable                :1;  // 2      | 0
  size_t cachable                  :1;  // 3      | 0
  size_t permissions               :2;  // 5:4    | 1
  size_t reserved                  :6;  // 11:6   | 0
  size_t page_ppn                  :20; // 31:12
} __attribute__((__packed__)) PageTableEntry;

static_assert(sizeof(PageTableEntry) == 4, "PageTableEntry is not 32 bit");

