//
// Created by mou on 4/2/23.
//

#ifndef __RFAAS_RDMA_ALLOCATOR_HPP__
#define __RFAAS_RDMA_ALLOCATOR_HPP__

#include <sys/mman.h>
#include <cstddef>
#include <rdmalib/buffer.hpp>
#include <rfaas/executor.hpp>

namespace rfaas {

  struct RdmaInfo {

  public:
    RdmaInfo(executor& executor, const int access, const int header_size=0)
        : executor(executor), access(access), header_size(header_size) {}
    const executor& executor;
    const int access;
    const int header_size = 0;
  };

  template<typename T>
  class RdmaAllocator {

  public:
    typedef T value_type;

    inline explicit RdmaAllocator(RdmaInfo& info) noexcept: _info(info) {}

    template<class U>
    constexpr RdmaAllocator(const RdmaAllocator<U> &) noexcept {}

    [[nodiscard]] inline T *allocate(const size_t &size) {
      if (size > std::numeric_limits<std::size_t>::max() / sizeof(T))
        throw std::bad_array_new_length();

      // Maybe we could directly call the memset function here
      if (auto buffer = new rdmalib::Buffer<char>(size, _info.header_size)) {
        report(buffer, size);
        buffer->register_memory(_info.executor._state.pd(), _info.access);
        return buffer;
      }
      throw std::bad_alloc();
    }

    inline void deallocate(T *p, std::size_t size) noexcept {
      report(p, size, 0);
      p->~T();
    }

  private:
    const RdmaInfo &_info;

    void report(T *p, std::size_t n, bool alloc = true) const {
      std::cout << (alloc ? "Alloc: " : "Dealloc: ") << sizeof(T) * n
                << " bytes at " << std::hex << std::showbase
                << reinterpret_cast<void *>(p) << std::dec << '\n';
    }
  };

  template<class T, class U>
  bool operator==(const RdmaAllocator<T> &, const RdmaAllocator<U> &) { return true; }

  template<class T, class U>
  bool operator!=(const RdmaAllocator<T> &, const RdmaAllocator<U> &) { return false; }
}

#endif //__RFAAS_RDMA_ALLOCATOR_HPP__
