
#ifndef __RDMALIB_UTIL_HPP__
#define __RDMALIB_UTIL_HPP__

#include <cstring>
#ifdef USE_LIBFABRIC
#include <rdma/fi_errno.h>
#endif
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
      #ifdef USE_LIBFABRIC
      spdlog::error("Expected zero, found: {}, message {}, errno {}, message {}", u, fi_strerror(std::abs(u)), errno, strerror(errno));
      #else
      display_message(display_strerror, msg);
      #endif
      traceback();
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

  template<typename StrType>
  const char* to_cstr(const StrType & str)
  {
    return str.c_str();
  }

	// Code borrowed from StackOverflow https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
	template<typename ... Args>
	std::string string_format( const std::string& format, Args... args )
	{
			int size_s = std::snprintf( nullptr, 0, format.c_str(), to_cstr(args)... ) + 1;
			if(size_s <= 0)
				return "";
			auto size = static_cast<size_t>(size_s);
			std::unique_ptr<char[]> buf(new char[size]);
			std::snprintf(buf.get(), size, format.c_str(), to_cstr(args)...);
			return std::string(buf.get(), buf.get() + size - 1);
	}

}}

#endif

