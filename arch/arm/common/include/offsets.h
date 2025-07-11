#pragma once

#include "board_constants.h"

/**
 * These are the basic offsets for our memory layout
 */

/**
 * Our image will be at 2gig virtual
 */
#define LINK_BASE 0x80000000

/**
 * physical ram starts at 0x0
 */
#define LOAD_BASE BOARD_LOAD_BASE

/**
 * this is the difference between link and load base
 */
#define PHYSICAL_TO_VIRTUAL_OFFSET (LINK_BASE - LOAD_BASE)

/**
 * returns the virtual address of a physical address by using the offset
 */
#define PHYSICAL_TO_VIRTUAL_BOOT(x) ((x) + PHYSICAL_TO_VIRTUAL_OFFSET)

/**
 * returns the physical address of a virtual address by using the offset
 */
#define VIRTUAL_TO_PHYSICAL_BOOT(x) ((x) - PHYSICAL_TO_VIRTUAL_OFFSET)

/**
 * The start of the ident mapping
 */
#define IDENT_MAPPING_START 0xC0000000U

/**
 * Only addresses below 2gig virtual belong to the user space
 */
#define USER_BREAK 0x80000000

