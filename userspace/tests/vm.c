#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"

int main(int argc, char *argv[]) {
    // Implement an interface to **nicely** start virtualized SWEB instances
    __syscall(sc_hypervisor_init_guest, 0, 0, 0, 0, 0);
    return 0;
}