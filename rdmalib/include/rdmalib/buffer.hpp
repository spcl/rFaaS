
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>
#include <utility>

#include <cereal/cereal.hpp>

//#ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <sys/uio.h>
//#else
struct ibv_pd;
struct ibv_mr;
struct ibv_sge;
//#endif

namespace rdmalib {

  template <typename MemoryRegion>
  struct ScatterGatherElement;

  namespace impl {

    // mregion - Memory region type: fid_mr* for libfabric, ibv_mr* for ibverbs
    // pdomain - Protected domain type: fid_domain* for libfabric, ibv_pd* for ibverbs
    // lkey - lkey type: void* for libfabric, uint32_t for ibverbs
    //template <typename Derived, typename MRegion, typename PDomain, typename LKey>
    template <typename Derived, typename MemoryRegion>
    struct Buffer {
    protected:
      uint32_t _size;
      uint32_t _header;
      uint32_t _bytes;
      uint32_t _byte_size;
      void* _ptr;
      MemoryRegion* _mr;
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
      MemoryRegion* mr() const;
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

      uint32_t rkey() const;

      ScatterGatherElement<MemoryRegion> sge(uint32_t size, uint32_t offset) const;
    };

    struct FabricBuffer : Buffer<FabricBuffer, fid_mr> {
      void destroy_buffer();
    };

    struct VerbsBuffer : Buffer<VerbsBuffer, ibv_mr> {
      void destroy_buffer();
    };
  }

  struct RemoteBuffer {
    uintptr_t addr;
    uint64_t rkey;
    uint32_t size;

    RemoteBuffer();
    // When accessing the remote buffer, we might not need to know the size.
    RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size = 0);

    #ifdef USE_LIBFABRIC
    RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size);
    #else
    RemoteBuffer(uintptr_t addr, uint32_t rkey, uint32_t size);
    #endif

    template<class Archive>
    void serialize(Archive & ar)
    {
      ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
    }
  };

  template<typename T, typename MemoryRegion>
  struct Buffer : impl::Buffer<Buffer<T, MemoryRegion>, MemoryRegion> {

    using ImplBuffer = impl::Buffer<Buffer<T, MemoryRegion>, MemoryRegion>;

    Buffer():
      ImplBuffer()
    {}

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(T * ptr, uint32_t size):
      ImplBuffer(ptr, size, sizeof(T))
    {}

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(void * ptr, uint32_t size):
      ImplBuffer(ptr, size, sizeof(T))
    {}

    Buffer(size_t size, size_t header = 0):
      ImplBuffer(size, sizeof(T), header)
    {}

    Buffer<T, MemoryRegion> & operator=(Buffer<T, MemoryRegion> && obj)
    {
      ImplBuffer::operator=(std::move(obj));
      return *this;
    }

    Buffer(const Buffer<T, MemoryRegion> & obj) = delete;
    Buffer(Buffer<T, MemoryRegion> && obj) = default;

    T* data() const
    {
      // void pointer arithmetic is not allowed
      return reinterpret_cast<T*>(static_cast<char*>(this->_ptr) + this->_header);
    }
  };

  template <typename MemoryRegion>
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
    ScatterGatherElement(const Buffer<T, MemoryRegion> & buf)
    {
      add(buf);
    }

    template<typename T>
    void add(const Buffer<T, MemoryRegion> & buf)
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
    void add(const Buffer<T, MemoryRegion> & buf, uint32_t size, size_t offset = 0)
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

