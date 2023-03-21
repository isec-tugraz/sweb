#include "ArchVMXHypervisor.h"

#include "ArchVMXDefinitions.h"
#include "ArchVMXGuest.h"
#include "kprintf.h"

// We copy the singleton pattern form the page manager
// You are free to change this if you dislike singletons

static ArchVMXHypervisor hv;

ArchHypervisor *ArchHypervisor::instance_ = nullptr;

ArchHypervisor *ArchHypervisor::instance() {
    if ( unlikely(!instance_) )
        instance_ = new (&hv) ArchVMXHypervisor();
    return instance_;
}

ArchVMXHypervisor::ArchVMXHypervisor()
  : guests_ {} {
    debug(HV_INIT, "created ArchVMXHypervisor ... TODO\n");
}

extern "C" void test_end();

__attribute__((naked))
void test_begin() {
  asm volatile(R"(
  .code32
  //your asm here

  .code64
  .global test_end
  test_end:
  )");
}

#define TEST_SIZE ((pointer)test_end - (pointer)test_begin)

//memcpy(dest, (void*)test_begin, TEST_SIZE); //to copy the code


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
    asm volatile("vmread %1, %0" : "=m"(ret) : "r"(field) : "cc");
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


const char *translateExitReason(const VmExit reason) {
  switch (reason) {
    case NMI:
      return "NMI";
    case EINT:
      return "EINT";
    case TRIPLE_FAULT:
      return "TRIPLE_FAULT";
    case INIT:
      return "INIT";
    case SIPI:
      return "SIPI";
    case IO_SMI:
      return "IO_SMI";
    case OTHER_SMI:
      return "OTHER_SMI";
    case INT_WINDOW:
      return "INT_WINDOW";
    case NMI_WINDOW:
      return "NMI_WINDOW";
    case TASK_SWITCH:
      return "TASK_SWITCH";
    case CPUID:
      return "CPUID";
    case GETSEC:
      return "GETSEC";
    case HLT:
      return "HLT";
    case INVD:
      return "INVD";
    case INVLPG:
      return "INVLPG";
    case RDPMC:
      return "RDPMC";
    case RDTSC:
      return "RDTSC";
    case RSM:
      return "RSM";
    case VMCALL:
      return "VMCALL";
    case VMCLEAR:
      return "VMCLEAR";
    case VMLAUNCH:
      return "VMLAUNCH";
    case VMPTRLD:
      return "VMPTRLD";
    case VMPTRST:
      return "VMPTRST";
    case VMREAD:
      return "VMREAD";
    case VMRESUME:
      return "VMRESUME";
    case VMWRITE:
      return "VMWRITE";
    case VMXON:
      return "VMXON";
    case CR:
      return "CR";
    case MOV_DB:
      return "MOV_DB";
    case IO_INST:
      return "IO_INST";
    case RDMSR:
      return "RDMSR";
    case WRMSR:
      return "WRMSR";
    case VM_ENTRY_FAIL_GUEST_STATE:
      return "VM_ENTRY_FAIL_GUEST_STATE";
    case VM_ENTRY_FAIL_MSR:
      return "VM_ENTRY_FAIL_MSR";
    case MWAIT:
      return "MWAIT";
    case MONITOR_TRAP_FLAG:
      return "MONITOR_TRAP_FLAG";
    case MONITOR:
      return "MONITOR";
    case PAUSE:
      return "PAUSE";
    case VM_ENTRY_FAIL_MACHINE_CHECK:
      return "VM_ENTRY_FAIL_MACHINE_CHECK";
    case TPR_BELOW_THRESHOLD:
      return "TPR_BELOW_THRESHOLD";
    case APIC_ACCESS:
      return "APIC_ACCESS";
    case VIRT_EOI:
      return "VIRT_EOI";
    case GDTR_IDTR:
      return "GDTR_IDTR";
    case LDTR_TR:
      return "LDTR_TR";
    case  EPT_VIOLATION:
      return "EPT_VIOLATION";
    case EPT_MISCONF:
      return "EPT_MISCONF";
    case INVEPT:
      return "INVEPT";
    case RDTSCP:
      return "RDTSCP";
    case VMX_PREEMPTION_TIMER:
      return "VMX_PREEMPTION_TIMER";
    case INVVPID:
      return "INVVPID";
    case WBINVD:
      return "WBINVD";
    case XSETBV:
     return "XSETBV";
    case APIC_WRITE:
      return "APIC_WRITE";
    case RDRAND:
      return "RDRAND";
    case INVPCID:
      return "INVPCID";
    case VMFUNC:
      return "VMFUNC";
    case ENCLS:
      return "ENCLS";
    case RDSEED:
      return "RDSEED";
    case PAGE_MOD_LOG:
      return "PAGE_MOD_LOG";
    case XSAVES:
      return "XSAVES";
    case XRSTORS:
      return "XRSTORS";
    case SPP:
      return "SPP";
    case UMWAIT:
      return "UMWAIT";
    case TPAUSE:
      return "TPAUSE";
    default:
      return "invalid exit reason";
  }
}
