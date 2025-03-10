#include <nonstd.h>
#include <stdio.h>
#include <types.h>
#include <nonstd.h>

unsigned int big_array[4ull*1024ull*1024ull];

// this is testcase 61
void memtest(size_t size)
{
  size /= 2048; // two accesses per page (array is unsigned int -> 4 byte step size)
  size_t errors = 0;
  printf("[INFO] fill the array\n");
  for (ssize_t i = size - 1; i >= 0; --i)
  {
    big_array[i * 512] = i*size*37; // fill two offsets per page with data that won't be there by chance
  }
  printf("[INFO] reaccess the array\n");
  for (ssize_t i = size - 1; i >= 0; --i)
  {
    if (big_array[i * 512] != i*size*37) // check whether the offsets still contain the same data
    {
      printf("[WARN] memory corrupted at page %zu\n",i);
      ++errors;
    }
  }
  if (errors == 0)
    printf("[PASS] done accessing the array without errors\n");
  else
    printf("[FAIL] done accessing the array with %zu errors\n", errors);
}


int main()
{
  memtest(3ull*1024ull*1024ull);
  return 0;
}
