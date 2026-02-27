#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"
#include "vm.h"
#include "types.h"
#include "assert.h"

void test_end();

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

#define TEST_SIZE ((size_t)test_end - (size_t)test_begin)

int main(int argc, char *argv[]) {
    assert(vm_launch_mem((void*)test_begin, TEST_SIZE) == 0);
    //assert(vm_launch_mb("/boot/boot/kernel.x"); // this is how you launch sweb
    return 0;
}
