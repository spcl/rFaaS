
#include <cstdint>
// traceback
#include <execinfo.h>

#include <spdlog/spdlog.h>

#include <rdmalib/util.hpp>

using std::size_t;

namespace rdmalib {namespace impl {

  void expect_true(bool flag)
  {
    expect_nonzero(flag);
  }

  void expect_false(bool flag)
  {
    expect_zero(flag);
  }

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
