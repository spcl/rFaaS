
#include <cstdint>
// traceback
#include <execinfo.h>

#include <spdlog/spdlog.h>

#include <rdmalib/util.hpp>

using std::size_t;

namespace rdmalib {namespace impl {

  void expect_true(bool flag, bool display_strerror, const std::string & msg)
  {
    expect_nonzero(flag, display_strerror, msg);
  }

  void expect_false(bool flag, bool display_strerror, const std::string & msg)
  {
    expect_zero(flag, display_strerror, msg);
  }

  void display_message(bool display_strerror, const std::string & user_msg)
  {

    if(!user_msg.empty())
      spdlog::error("Reason: {}", user_msg);

    if(display_strerror)
      spdlog::error("Errno {}, system error message {}", errno, strerror(errno));

    traceback();
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
