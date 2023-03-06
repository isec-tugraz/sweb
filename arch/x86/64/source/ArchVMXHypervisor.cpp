#include "ArchVMXHypervisor.h"

#include "ArchVMXDefinitions.h"
#include "ArchVMXGuest.h"
#include "kprintf.h"

// We copy the singleton pattern form the page manager
// You are free to change this if you dislike singletons

static ArchVMXHypervisor hv;

ArchHyperVisor *ArchHyperVisor::instance_ = nullptr;

ArchHyperVisor *ArchHyperVisor::instance() {
    if ( unlikely(!instance_) )
        instance_ = new (&hv) ArchVMXHypervisor();
    return instance_;
}

ArchVMXHypervisor::ArchVMXHypervisor()
  : guests_ {} {
    debug(HV_INIT, "created ArchVMXHypervisor ... TODO\n");
}

void ArchVMXHypervisor::init_guest(/*TODO*/) {
    // TODOs
    size_t id = 0;
    guests_[id] = ArchVMXGuest();
    debug(HV_INIT, "added a guest ... TODO\n");

    uint64 vmx_basic = rdmsr(MSR_IA32_VMX_BASIC);

    debug(HV_INIT, "vmx_basic=0x%lx\n", vmx_basic);

    dump_vmx_msrs();

    // will only work after setting up vmx and loading a control struct
    // dump_vmx_config();
}

void ArchVMXHypervisor::run_guest(/*TODO*/) {

    bool reenter = false;

    do {

        // prepare guest
        // SAVE registers!

        asm volatile(/*TODO*/ "" ::: /*HINT: CLOBBERS ARE AWESOME*/);

        // RESTORE registers!
        // handle exit
        VmExit exit_reason = /*TODO*/ VmExit::INT_WINDOW;

        switch ( exit_reason ) {
            case VmExit::INT_WINDOW:
                /*TODO */
                break;
            default:
                reenter = false;
        }
        // decide if return oder continue
    } while ( reenter );
}

void vmwrite(uint64 field, uint64 value) {
    /*TODO*/
    (void)(field + value);
}

uint64 vmread(uint64 field) {
    uint64 ret;
    asm volatile("vmread %1, %0" : "=m"(ret) : "r"(field));
    return ret;
}

void wrmsr(uint32 msr, uint64 value) {
    asm volatile("wrmsr" : : "a"(value), "d"(value >> 32), "c"(msr));
}

uint64 rdmsr(uint32 msr) {
    uint64 hi, lo;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (hi << 32) | lo;
}
