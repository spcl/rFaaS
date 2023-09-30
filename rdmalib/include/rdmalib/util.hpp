
#ifndef __RDMALIB_UTIL_HPP__
#define __RDMALIB_UTIL_HPP__

#include <spdlog/spdlog.h>

namespace rdmalib { namespace impl {

  void traceback();

  void expect_true(bool flag, bool display_strerror = true, const std::string & msg = "");
  void expect_false(bool flag, bool display_strerror = true, const std::string & msg = "");
  void display_message(bool display_strerror, const std::string & msg);

  template<typename U>
  void expect_zero(U && u, bool display_strerror = true, const std::string & msg = "")
  {
    if(u) {
      spdlog::error("Expected zero, found: {}", u);
      display_message(display_strerror, msg);
    }
    assert(!u);
  }

  template<typename U>
  void expect_nonzero(U && u, bool display_strerror = true, const std::string & msg = "")
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", u);
      display_message(display_strerror, msg);
    }
    assert(u);
  }

  template<typename U>
  void expect_nonzero(U * u, bool display_strerror = true, const std::string & msg = "")
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", fmt::ptr(u));
      display_message(display_strerror, msg);
    }
    assert(u);
  }

  template<typename U>
  void expect_nonnegative(U && u, bool display_strerror = true, const std::string & msg = "")
  {
    if(u < 0) {
      spdlog::error("Expected non-negative number, found: {}", u);
      display_message(display_strerror, msg);
    }
    assert(u >= 0);
  }

  template<typename U>
  void expect_nonnull(U* ptr, bool display_strerror = true, const std::string & msg = "")
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      display_message(display_strerror, msg);
    }
    assert(ptr);
  }

  template<typename U, typename F>
  void expect_nonnull(U* ptr, F && f, bool display_strerror = true, const std::string & msg = "")
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      f();
      display_message(display_strerror, msg);
    }
    assert(ptr);
  }

}}

#endif

