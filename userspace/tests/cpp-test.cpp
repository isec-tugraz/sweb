#include "stdio.h"
#include "stdlib.h"

class TestClass
{
private:

  int num_;

public:

  TestClass() = default;
  TestClass(int num) : num_(num) {}
  
  void helloWorld()
  {
    printf("Hello world with num %d\n", num_);
  }
};

int main()
{
  TestClass test(123);

  test.helloWorld();

  TestClass *test_p = new TestClass();
  printf("%p\n", test_p);

  exit(0);
}