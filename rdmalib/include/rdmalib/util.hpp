
#ifndef __RDMALIB_UTIL_HPP__
#define __RDMALIB_UTIL_HPP__

#include <cstring>
#include <rdma/fi_errno.h>
#include <spdlog/spdlog.h>

namespace rdmalib { namespace impl {

  void traceback();

  void expect_true(bool flag);
  void expect_false(bool flag);

  template<typename U>
  void expect_zero(U && u)
  {
    if(u) {
      #ifdef USE_LIBFABRIC
      spdlog::error("Expected zero, found: {}, message {}, errno {}, message {}", u, fi_strerror(std::abs(u)), errno, strerror(errno));
      #else
      spdlog::error("Expected zero, found: {}, errno {}, message {}", u, errno, strerror(errno));
      #endif
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
  void expect_nonzero(U * u)
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", fmt::ptr(u));
      traceback();
    }
    assert(u);
  }

  template<typename U>
  void expect_nonnegative(U && u)
  {
    if(u < 0) {
      spdlog::error("Expected non-negative number, found: {}", u);
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

  template<typename U, typename F>
  void expect_nonnull(U* ptr, F && f)
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      f();
      traceback();
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

