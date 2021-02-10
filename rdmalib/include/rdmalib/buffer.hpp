
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>
#include <utility>

struct ibv_pd;
struct ibv_mr;

namespace rdmalib {

  namespace impl {

    // move non-template methods from header
    struct Buffer {
    protected:
      size_t _size;
      size_t _header;
      size_t _bytes;
      ibv_mr* _mr;
      void* _ptr;

      Buffer();
      Buffer(size_t size, size_t byte_size, size_t header);
      Buffer(Buffer &&);
      Buffer & operator=(Buffer && obj);
      ~Buffer();
    public:
      uintptr_t address() const;
      void* ptr() const;
      size_t data_size() const;
      size_t size() const;
      void register_memory(ibv_pd *pd, int access);
      uint32_t lkey() const;
      uint32_t rkey() const;
    };

  }

  template<typename T>
  struct Buffer : impl::Buffer{

    Buffer():
      impl::Buffer()
    {}

    Buffer(size_t size, size_t header = 0):
      impl::Buffer(size, sizeof(T), header)
    {}

    Buffer<T> & operator=(Buffer<T> && obj)
    {
      impl::Buffer::operator=(std::move(obj));
      return *this;
    }

    Buffer(const Buffer<T> & obj) = delete;
    Buffer(Buffer<T> && obj) = default;

    T* data() const
    {
      return static_cast<T*>(this->_ptr) + this->_header;
    }
  };
}

#endif

