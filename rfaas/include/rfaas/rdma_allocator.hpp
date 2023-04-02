//
// Created by mou on 4/2/23.
//

#ifndef __RFAAS_RDMA_ALLOCATOR_HPP__
#define __RFAAS_RDMA_ALLOCATOR_HPP__

#include <cstddef>
#include <rdmalib/buffer.hpp>
#include <rfaas/executor.hpp>

namespace rfaas {
  template<typename T>
  class RdmaAllocator {
  private:
    const executor &_executor;

  public:
    inline explicit RdmaAllocator(const executor &executor) noexcept: _executor(executor) {}

    // inline T *allocate(const std::size_t &, const int &, int = 0);
    inline T *allocate(const std::size_t &size, const int &access, int header=0) {
      if (size > std::size_t(-1) / sizeof(T))
        throw std::bad_alloc();

      auto buffer = new rdmalib::Buffer<char>(size, header);
      buffer->register_memory(_executor._state.pd(), access);
      std::cout << "allocate memory by RdmaAllocator" << std::endl;
      return buffer;
    }

    inline void deallocate(T *p, std::size_t n) noexcept {
      operator delete(p);
    }
  };
}

#endif //__RFAAS_RDMA_ALLOCATOR_HPP__
