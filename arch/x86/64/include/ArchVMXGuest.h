#pragma once

#include "ArchGuest.h"
#include "ArchVMXDefinitions.h"

class ArchVMXMemory {
    // these address translations are important
    // therefore we suggest an interface like this
    // Note: the address translations depends on the active guest
    // and the hypervisor!

    // hints
    uint64 guest_cr3;
    uint64 host_ept;

  public:
    gpa_t translate_gva_to_gpa(gva_t guest_virtual_address);
    hpa_t translate_gpa_to_hpa(gpa_t guest_physical_address);
    hpa_t translate_gva_to_hpa(gva_t guest_virtual_address);
};

// this class represents the virtualized guests
// but stuff that is specific to the guest here
class ArchVMXGuest : public ArchGuest {

    ArchVMXMemory memory_;

  public:
    // TODO

  private:
};