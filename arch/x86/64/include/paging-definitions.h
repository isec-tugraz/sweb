#pragma once

#include "types.h"

#define PAGE_MAP_LEVEL_4_ENTRIES 512
#define PAGE_DIR_POINTER_TABLE_ENTRIES 512
#define PAGE_DIR_ENTRIES 512
#define PAGE_TABLE_ENTRIES 512
#define PAGE_SIZE 4096

#define PAGE_INDEX_OFFSET_BITS 12



// Constants for page fault handling
#define ERROR_MASK            0x00000007
#define PAGE_FAULT_USER       0x00000004
#define PAGE_FAULT_WRITEABLE  0x00000002
#define PAGE_FAULT_PRESENT    0x00000001

typedef struct
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t ignored_3                 :1;
  size_t size                      :1; // must be 0
  size_t ignored_2                 :4;
  size_t page_ppn                  :28;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__)) PageMapLevel4Entry;

static_assert(sizeof(PageMapLevel4Entry) == 8, "PageMapLevel4Entry is not 64 bit");

struct PageDirPointerTablePageDirEntry
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t ignored_3                 :1;
  size_t size                      :1; // 0 means page directory mapped
  size_t ignored_2                 :4;
  size_t page_ppn                  :28;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__));

static_assert(sizeof(PageDirPointerTablePageDirEntry) == 8, "PageDirPointerTablePageDirEntry is not 64 bit");

struct PageDirPointerTablePageEntry
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t dirty                     :1;
  size_t size                      :1; // 1 means 1gb page mapped
  size_t global                    :1;
  size_t ignored_2                 :3;
  size_t pat                       :1;
  size_t reserved_2                :17; // must be 0
  size_t page_ppn                  :10;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__));

static_assert(sizeof(PageDirPointerTablePageEntry) == 8, "PageDirPointerTablePageEntry is not 64 bit");

typedef union
{
  struct PageDirPointerTablePageDirEntry pd;
  struct PageDirPointerTablePageEntry page;
} __attribute__((__packed__)) PageDirPointerTableEntry;

struct PageDirPageTableEntry
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t ignored_3                 :1;
  size_t size                      :1; // 0 means page table mapped
  size_t ignored_2                 :4;
  size_t page_ppn                  :28;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__));

static_assert(sizeof(PageDirPageTableEntry) == 8, "PageDirPageTableEntry is not 64 bit");

struct PageDirPageEntry
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t dirty                     :1;
  size_t size                      :1;  // 1 means 2mb page mapped
  size_t global                    :1;
  size_t ignored_2                 :3;
  size_t pat                       :1;
  size_t reserved_2                :8; // must be 0
  size_t page_ppn                  :19;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__));

static_assert(sizeof(PageDirPageEntry) == 8, "PageDirPageEntry is not 64 bit");

typedef union
{
  struct PageDirPageTableEntry pt;
  struct PageDirPageEntry page;
} __attribute__((__packed__)) PageDirEntry;

typedef struct
{
  size_t present                   :1;
  size_t writeable                 :1;
  size_t user_access               :1;
  size_t write_through             :1;
  size_t cache_disabled            :1;
  size_t accessed                  :1;
  size_t dirty                     :1;
  size_t size                      :1;
  size_t global                    :1;
  size_t ignored_2                 :3;
  size_t page_ppn                  :28;
  size_t reserved_1                :12; // must be 0
  size_t ignored_1                 :11;
  size_t execution_disabled        :1;
} __attribute__((__packed__)) PageTableEntry;

static_assert(sizeof(PageTableEntry) == 8, "PageTableEntry is not 64 bit");
