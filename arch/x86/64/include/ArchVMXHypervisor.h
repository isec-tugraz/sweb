#pragma once

#include "ArchHypervisor.h"
#include "ArchVMXDefinitions.h"
#include "ArchVMXGuest.h"
#include "umap.h"

// this class represents the hypervisor guests
// the hypervisor manages the virtualized guests

class ArchVMXHypervisor : public ArchHyperVisor {

    ustl::map<size_t, ArchVMXGuest> guests_;

  public:
    ArchVMXHypervisor();

    virtual void init_guest(/*TODO*/);

    virtual void run_guest(/*TODO*/);

    virtual ~ArchVMXHypervisor() {
    }

  private:
    // these address translations are important
    // therefore we suggest an interface like this
    // Note: the address translations depends on the active guest
    // and the hypervisor!

    // maybe reimplement somethjing like ArchMemory ...
    gpa_t translate_gva_to_gpa(ArchVMXGuest &guest, gva_t guest_virtual_address);
    hpa_t translate_gpa_to_hpa(ArchVMXGuest &guest, gpa_t guest_physical_address);

    hpa_t translate_gva_to_hpa(ArchVMXGuest &guest, gva_t guest_virtual_address) {
        gpa_t gpa = translate_gva_to_gpa(guest, guest_virtual_address);
        hpa_t hpa = translate_gpa_to_hpa(guest, gpa);
        return hpa;
    }
};