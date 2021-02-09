
#include <cstdint>
// traceback
#include <execinfo.h>

#include <spdlog/spdlog.h>

using std::size_t;

namespace rdmalib {namespace impl {
  void traceback()
  {
    void* array[10];
    size_t size = backtrace(array, 10);
    char ** trace = backtrace_symbols(array, size);
    for(size_t i = 0; i < size; ++i)
      spdlog::warn("Traceback {}: {}", i, trace[i]);
    free(trace);
  }
}}
