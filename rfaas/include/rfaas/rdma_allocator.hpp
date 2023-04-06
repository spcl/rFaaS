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
    RdmaInfo(executor &executor, const int &access, const int &header_size = 0)
        : executor(executor), access(access), header_size(header_size) {}

    const executor &executor;
    const int &access;
    const int &header_size = 0;
  };

  template<typename T>
  class RdmaAllocator {

  public:
    typedef T value_type;

    inline constexpr explicit RdmaAllocator(RdmaInfo &info) noexcept: _info(info) {}

    template<class U>
    inline constexpr explicit RdmaAllocator(const RdmaAllocator<U> &) noexcept {}

    [[nodiscard]] inline T *allocate(const size_t &size) {
      if (size > std::numeric_limits<std::size_t>::max() / sizeof(T))
        throw std::bad_array_new_length();

      if (auto p = static_cast<T *>(std::malloc(size * sizeof(T) + _info.header_size))) {
        report(p, size * sizeof(T) + _info.header_size);
        return p;
      }
      throw std::bad_alloc();
    }

    template<typename U, typename... Args>
    inline void construct(U *p, Args &&... args) {
      ::new(p) U(std::forward<Args>(args)...);
      p->register_memory(_info.executor._state.pd(), _info.access);
    }

    inline void deallocate(T *p, std::size_t size) noexcept {
      report(p, size, 0);
      std::free(p);
    }

    template<typename U>
    struct rebind {
      using other = RdmaAllocator<U>;
    };

  private:
    const RdmaInfo &_info;

    inline void report(T *p, std::size_t size, bool alloc = true) const {
      std::cout << (alloc ? "Alloc: " : "Dealloc: ") << size
                << " bytes at " << std::hex << std::showbase
                << reinterpret_cast<void *>(p) << std::dec << '\n';
    }
  };

  template<class T, class U>
  inline bool operator==(const RdmaAllocator<T> &, const RdmaAllocator<U> &) { return true; }

  template<class T, class U>
  inline bool operator!=(const RdmaAllocator<T> &, const RdmaAllocator<U> &) { return false; }
}

#endif //__RFAAS_RDMA_ALLOCATOR_HPP__