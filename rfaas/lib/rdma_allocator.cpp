//
// Created by mou on 4/2/23.
//
#include <memory>

#include <rfaas/rdma_allocator.hpp>

namespace rfaas {

  template<typename T>
  inline T *RdmaAllocator<T>::allocate(const std::size_t &size, const int &access, int header) {
    if (size > std::size_t(-1) / sizeof(T))
      throw std::bad_alloc();

    rdmalib::Buffer<char> buffer(size, header);
    buffer.register_memory(_executor._state.pd(), access);

    return buffer;
  }

  template<typename T>
  inline void RdmaAllocator<T>::deallocate(T *p, std::size_t n) noexcept {
    operator delete(p);
  }
}