
#include <cstdint>

extern "C" uint32_t empty(void* args, uint32_t size, void* res)
{
  int* src = static_cast<int*>(args), *dest = static_cast<int*>(res);
  *dest = *src;
  return size;
}

