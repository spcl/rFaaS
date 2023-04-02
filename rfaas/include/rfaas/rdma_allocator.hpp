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

    inline T *allocate(const std::size_t &, const int &, int = 0);

    inline void deallocate(T *p, std::size_t n) noexcept;
  };
}

#endif //__RFAAS_RDMA_ALLOCATOR_HPP__
