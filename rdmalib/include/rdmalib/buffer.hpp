
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>
#include <utility>

#include <cereal/cereal.hpp>

struct ibv_pd;
struct ibv_mr;
struct ibv_sge;

namespace rdmalib {

  struct ScatterGatherElement;

  struct BufferInformation {
    uint64_t r_addr;
    uint32_t r_key;
  };

  namespace impl {

    // move non-template methods from header
    struct Buffer {
    protected:
      uint32_t _size;
      uint32_t _header;
      uint32_t _bytes;
      uint32_t _byte_size;
      void* _ptr;
      ibv_mr* _mr;
      bool _own_memory;

      Buffer();
      Buffer(void* ptr, uint32_t size, uint32_t byte_size);
      Buffer(uint32_t size, uint32_t byte_size, uint32_t header);
      Buffer(Buffer &&);
      Buffer & operator=(Buffer && obj);
      ~Buffer();
    public:
      uintptr_t address() const;
      void* ptr() const;
      ibv_mr* mr() const;
      uint32_t data_size() const;
      uint32_t size() const;
      uint32_t bytes() const;
      void register_memory(ibv_pd *pd, int access);
      uint32_t lkey() const;
      uint32_t rkey() const;
      ScatterGatherElement sge(uint32_t size, uint32_t offset) const;
    };

  }

  struct RemoteBuffer {
    uintptr_t addr;
    uint32_t rkey;
    uint32_t size;

    RemoteBuffer();
    // When accessing the remote buffer, we might not need to know the size.
    RemoteBuffer(uintptr_t addr, uint32_t rkey, uint32_t size = 0);

    template<class Archive>
    void serialize(Archive & ar)
    {
      ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
    }
  };

  template<typename T>
  struct Buffer : impl::Buffer{

    typedef T value_type;

    Buffer():
      impl::Buffer()
    {}

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(T * ptr, uint32_t size):
      impl::Buffer(ptr, size, sizeof(T))
    {}

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(void * ptr, uint32_t size):
      impl::Buffer(ptr, size, sizeof(T))
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
      // void pointer arithmetic is not allowed
      return reinterpret_cast<T*>(static_cast<char*>(this->_ptr) + this->_header);
    }

    T& operator[](int idx) const
    {
      // void pointer arithmetic is not allowed
      return reinterpret_cast<T*>(static_cast<char*>(this->_ptr) + this->_header)[idx];
    }
  };

  struct ScatterGatherElement {
    // smallvector in practice
    mutable std::vector<ibv_sge> _sges;

    ScatterGatherElement();

    ScatterGatherElement(uint64_t addr, uint32_t bytes, uint32_t lkey);

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf)
    {
      add(buf);
    }

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf, int elements)
    {
      add(buf, elements);
    }

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf, int elements, size_t offset)
    {
      add(buf, elements, offset);
    }

    template<typename T>
    void add(const Buffer<T> & buf)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), buf.bytes(), buf.lkey()});
    }

    template<typename T>
    void add(const Buffer<T> & buf, int elements)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), sizeof(T) * elements, buf.lkey()});
    }

    template<typename T>
    void add(const Buffer<T> & buf, uint32_t size, size_t offset = 0)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address() + offset, size, buf.lkey()});
    }

    ibv_sge * array() const;
    size_t size() const;
  };
}

#endif

