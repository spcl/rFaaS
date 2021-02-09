
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>


struct ibv_pd;
struct ibv_mr;

namespace rdmalib {

  namespace impl {

    // move non-template methods from header
    struct Buffer {
    protected:
      size_t _size;
      size_t _bytes;
      ibv_mr* _mr;
      void* _ptr;

      Buffer(size_t size, size_t byte_size);
      ~Buffer();
    public:
      uintptr_t ptr() const;
      size_t size() const;
      void register_memory(ibv_pd *pd, int access);
      uint32_t lkey() const;
      uint32_t rkey() const;
    };

  }

  template<typename T>
  struct Buffer : impl::Buffer{

    Buffer(size_t size):
      impl::Buffer(size, sizeof(T))
    {}

    T* data() const
    {
      return static_cast<T*>(this->_ptr);
    }
  };
}

#endif

