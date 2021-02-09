
#include <spdlog/spdlog.h>

namespace rdmalib { namespace impl {

  void traceback();

  template<typename U>
  void expect_zero(U && u)
  {
    if(u) {
      spdlog::error("Expected zero, found: {}", u);
      traceback();
    }
    assert(!u);
  }

  template<typename U>
  void expect_nonzero(U && u)
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", u);
      traceback();
    }
    assert(u);
  }

  template<typename U>
  void expect_nonnull(U* ptr)
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      traceback();
    }
    assert(ptr);
  }

}}
