
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

  template <typename Derived, typename SGE, typename MemoryRegion, typename Domain, typename LKey, typename RKey>
  struct ScatterGatherElement;

  struct FabricScatterGatherElement;
  struct VerbsScatterGatherElement;

  namespace impl {

    // mregion - Memory region type: fid_mr* for libfabric, ibv_mr* for ibverbs
    // pdomain - Protected domain type: fid_domain* for libfabric, ibv_pd* for ibverbs
    // lkey - lkey type: void* for libfabric, uint32_t for ibverbs
    // rkey - rkey type: u64 for libfabric, u32 for ibverbs
    template <typename Derived, typename MemoryRegion, typename Domain, typename LKey, typename RKey>
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
      void register_memory(Domain *pd, int access)
      {
        static_cast<Derived*>(this)->_register_memory(pd, access);
      }
      LKey lkey() const
      {
        return static_cast<const Derived*>(this)->_lkey();
      }
      RKey rkey() const
      {
        return static_cast<Derived*>(this)->_rkey();
      }

      template <typename SGE>
      ScatterGatherElement<Derived, SGE, MemoryRegion, Domain, LKey, RKey> sge(uint32_t size, uint32_t offset) const
      {
        return static_cast<Derived*>(this)->_sge(size, offset);
      }
    };

    struct FabricBuffer : Buffer<FabricBuffer, fid_mr, fid_domain, void*, uint64_t> {
      void _register_memory(fid_domain *pd, int access);

      void *_lkey() const;
      uint64_t _rkey() const;

      FabricScatterGatherElement _sge(uint32_t size, uint32_t offset) const;

      void destroy_buffer();
    };

    struct VerbsBuffer : Buffer<VerbsBuffer, ibv_mr, ibv_pd, uint32_t, uint32_t> {
      void register_memory(ibv_pd* pd, int access);

      uint32_t _lkey() const;
      uint32_t _rkey() const;

      VerbsScatterGatherElement _sge(uint32_t size, uint32_t offset) const;

      void destroy_buffer();
    };
  }

  // RKey - rkey type: u32 for ibverbs, u64 for libfabric
  template <typename RKey>
  struct RemoteBuffer {
    uintptr_t addr;
    uint64_t rkey;
    uint32_t size;

    RemoteBuffer();
    // When accessing the remote buffer, we might not need to know the size.
    RemoteBuffer(uintptr_t addr, RKey rkey, uint32_t size = 0);

    template<class Archive>
    void serialize(Archive & ar)
    {
      ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
    }
  };

  template<typename T, typename MemoryRegion, typename Domain, typename LKey, typename RKey>
  struct Buffer : impl::Buffer<Buffer<T, MemoryRegion, Domain, LKey, RKey>, MemoryRegion, Domain, LKey, RKey> {

    using Self = Buffer<T, MemoryRegion, Domain, LKey, RKey>;
    using ImplBuffer = impl::Buffer<Self, MemoryRegion, Domain, LKey, RKey>;

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

    Self & operator=(Self && obj)
    {
      ImplBuffer::operator=(std::move(obj));
      return *this;
    }

    Buffer(const Self & obj) = delete;
    Buffer(Self && obj) = default;

    T* data() const
    {
      // void pointer arithmetic is not allowed
      return reinterpret_cast<T*>(static_cast<char*>(this->_ptr) + this->_header);
    }
  };

  // SGE - the scatter gather element type: iovec for libfabric, ibv_sge for verbs
  template <typename Derived, typename SGE, typename MemoryRegion, typename Domain, typename LKey, typename RKey>
  struct ScatterGatherElement {
    // smallvector in practice
    mutable std::vector<SGE> _sges;

    ScatterGatherElement();

    ScatterGatherElement(uint64_t addr, uint32_t bytes, LKey lkey)
    {
      static_cast<Derived*>(this);
    }

    template<typename T>
    ScatterGatherElement(const Buffer<T, MemoryRegion, Domain, LKey, RKey> & buf)
    {
      add(buf);
    }

    template<typename T>
    void add(const Buffer<T, MemoryRegion, Domain, LKey, RKey> & buf)
    {
      static_cast<Derived*>(this)->_add(buf);
    }

    template<typename T>
    void add(const Buffer<T, MemoryRegion, Domain, LKey, RKey> & buf, uint32_t size, size_t offset = 0)
    {
      static_cast<Derived*>(this)->_add(buf, size, offset);
    }

    SGE *array() const
    {
      return static_cast<Derived*>(this)->_array();
    }

    size_t size() const;
  };


  struct FabricScatterGatherElement : ScatterGatherElement<FabricScatterGatherElement, iovec, fid_mr, fid_domain, void*, uint64_t>
  {

    mutable std::vector<void *> _lkeys;

    FabricScatterGatherElement(uint64_t addr, uint32_t bytes, void *lkey);

    template<typename T>
    void _add(const Buffer<T, fid_mr, fid_domain, void*, uint64_t> & buf)
    {
      _sges.push_back({(void *)buf.address(), (size_t)buf.bytes()});
      _lkeys.push_back(buf.lkey());
    }

    template<typename T>
    void _add(const Buffer<T, fid_mr, fid_domain, void*, uint64_t> & buf, uint32_t size, size_t offset = 0)
    {
      _sges.push_back({(void *)(buf.address() + offset), (size_t)size});
      _lkeys.push_back(buf.lkey());
    }

    iovec *_array() const;
    void **lkeys() const;
  };

  struct VerbsScatterGatherElement : ScatterGatherElement<VerbsScatterGatherElement, ibv_sge, ibv_mr, ibv_pd, uint32_t, uint32_t>
  {

    VerbsScatterGatherElement(uint64_t addr, uint32_t bytes, uint32_t lkey);

    template<typename T>
    void _add(const Buffer<T, ibv_mr, ibv_pd, uint32_t, uint32_t> & buf)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), buf.bytes(), buf.lkey()});
    }

    template<typename T>
    void _add(const Buffer<T, ibv_mr, ibv_pd, uint32_t, uint32_t> & buf, uint32_t size, size_t offset = 0)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address() + offset, size, buf.lkey()});
    }

    ibv_sge *_array() const;
  };
}

#endif

