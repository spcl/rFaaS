
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>
#include <utility>

#include <cereal/cereal.hpp>

#ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <sys/uio.h>
#else
struct ibv_pd;
struct ibv_mr;
struct ibv_sge;
#endif

namespace rdmalib {

  struct ScatterGatherElement;

  namespace impl {

    // move non-template methods from header
    struct Buffer {
    protected:
      uint32_t _size;
      uint32_t _header;
      uint32_t _bytes;
      uint32_t _byte_size;
      void* _ptr;
      #ifdef USE_LIBFABRIC
      fid_mr* _mr;
      #else
      ibv_mr* _mr;
      #endif
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
      #ifdef USE_LIBFABRIC
      fid_mr* mr() const;
      #else
      ibv_mr* mr() const;
      #endif
      uint32_t data_size() const;
      uint32_t size() const;
      uint32_t bytes() const;
      #ifdef USE_LIBFABRIC
      void register_memory(fid_domain *pd, int access);
      #else
      void register_memory(ibv_pd *pd, int access);
      #endif
      #ifdef USE_LIBFABRIC
      void *lkey() const;
      #else
      uint32_t lkey() const;
      #endif
      uint64_t rkey() const;
      ScatterGatherElement sge(uint32_t size, uint32_t offset) const;
    };

  }

  struct RemoteBuffer {
    uintptr_t addr;
    uint64_t rkey;
    uint32_t size;

    RemoteBuffer();
    // When accessing the remote buffer, we might not need to know the size.
    RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size = 0);

    template<class Archive>
    void serialize(Archive & ar)
    {
      ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
    }
  };

  template<typename T>
  struct Buffer : impl::Buffer{

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
  };

  struct ScatterGatherElement {
    // smallvector in practice
    #ifdef USE_LIBFABRIC
    mutable std::vector<iovec> _sges;
    mutable std::vector<void *> _lkeys;
    #else
    mutable std::vector<ibv_sge> _sges;
    #endif

    ScatterGatherElement();

    #ifdef USE_LIBFABRIC
    ScatterGatherElement(uint64_t addr, uint32_t bytes, void *lkey);
    #else
    ScatterGatherElement(uint64_t addr, uint32_t bytes, uint32_t lkey);
    #endif

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf)
    {
      add(buf);
    }

    template<typename T>
    void add(const Buffer<T> & buf)
    {
      #ifdef USE_LIBFABRIC
      _sges.push_back({(void *)buf.address(), (size_t)buf.bytes()});
      _lkeys.push_back(buf.lkey());
      #else
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), buf.bytes(), buf.lkey()});
      #endif
    }

    template<typename T>
    void add(const Buffer<T> & buf, uint32_t size, size_t offset = 0)
    {
      #ifdef USE_LIBFABRIC
      _sges.push_back({(void *)(buf.address() + offset), (size_t)size});
      _lkeys.push_back(buf.lkey());
      #else
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address() + offset, size, buf.lkey()});
      #endif
    }

    #ifdef USE_LIBFABRIC
    iovec *array() const;
    void **lkeys() const;
    #else
    ibv_sge * array() const;
    #endif
    size_t size() const;
  };
}

#endif

