
// mmap
#include <sys/mman.h>

#ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#else
#include <infiniband/verbs.h>
#endif

#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib { namespace impl {

  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>::Buffer():
    _size(0),
    _header(0),
    _bytes(0),
    _byte_size(0),
    _ptr(nullptr),
    _mr(nullptr),
    _own_memory(false)
  {}

  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>::Buffer(Buffer && obj):
    _size(obj._size),
    _header(obj._header),
    _bytes(obj._bytes),
    _byte_size(obj._byte_size),
    _ptr(obj._ptr),
    _mr(obj._mr),
    _own_memory(obj._own_memory)
  {
    obj._size = obj._bytes = obj._header = 0;
    obj._ptr = obj._mr = nullptr;
  }

  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>& Buffer<Derived, MemoryRegion>::operator=(Buffer<Derived, MemoryRegion> && obj)
  {
    _size = obj._size;
    _bytes = obj._bytes;
    _bytes = obj._byte_size;
    _header = obj._header;
    _ptr = obj._ptr;
    _mr = obj._mr;
    _own_memory = obj._own_memory;

    obj._size = obj._bytes = 0;
    obj._ptr = obj._mr = nullptr;
    return *this;
  }

  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>::Buffer(uint32_t size, uint32_t byte_size, uint32_t header):
    _size(size),
    _header(header),
    _bytes(size * byte_size + header),
    _byte_size(byte_size),
    _mr(nullptr),
    _own_memory(true)
  {
    //size_t alloc = _bytes;
    //if(alloc < 4096) {
    //  alloc = 4096;
    //  spdlog::warn("Page too small, allocating {} bytes", alloc);
    //}
    // page-aligned address for maximum performance
    _ptr = mmap(nullptr, _bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    SPDLOG_DEBUG(
      "Allocated {} bytes, address {}",
      _bytes, fmt::ptr(_ptr)
    );
  }

  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>::Buffer(void* ptr, uint32_t size, uint32_t byte_size):
    _size(size),
    _header(0),
    _bytes(size * byte_size),
    _byte_size(byte_size),
    _ptr(ptr),
    _mr(nullptr),
    _own_memory(false)
  {
    SPDLOG_DEBUG(
      "Allocated {} bytes, address {}",
      _bytes, fmt::ptr(_ptr)
    );
  }
  
  template <typename Derived, typename MemoryRegion>
  Buffer<Derived, MemoryRegion>::~Buffer()
  {
    SPDLOG_DEBUG(
      "Deallocate {} bytes, mr {}, ptr {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr)
    );
    static_cast<Derived*>(this)->destroy_buffer();
  }

  void FabricBuffer::destroy_buffer()
  {
    if(_mr)
      impl::expect_zero(fi_close(&_mr->fid));
    if(_own_memory)
      munmap(_ptr, _bytes);
  }

  void VerbsBuffer::destroy_buffer()
  {
    if(_mr)
      ibv_dereg_mr(_mr);
    if(_own_memory)
      munmap(_ptr, _bytes);
  }

  #ifdef USE_LIBFABRIC // requires proc domain refactor (up next)
  void Buffer::register_memory(fid_domain *pd, int access)
  {
    int ret = fi_mr_reg(pd, _ptr, _bytes, access, 0, 0, 0, &_mr, nullptr);
    impl::expect_zero(ret);
    SPDLOG_DEBUG(
      "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr), fmt::ptr(fi_mr_desc(_mr)), fi_mr_key(_mr)
    );
  }
  #else

  template <typename Derived, typename MemoryRegion>
  void Buffer<Derived, MemoryRegion>::register_memory(ibv_pd* pd, int access)
  {
    _mr = ibv_reg_mr(pd, _ptr, _bytes, access);
    impl::expect_nonnull(_mr);
    SPDLOG_DEBUG(
      "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey
    );
  }
  #endif

  template <typename Derived, typename MemoryRegion>
  MemoryRegion* Buffer<Derived, MemoryRegion>::mr() const
  {
    return this->_mr;
  }

  template <typename Derived, typename MemoryRegion>
  uint32_t Buffer<Derived, MemoryRegion>::data_size() const
  {
    return this->_size;
  }

  template <typename Derived, typename MemoryRegion>
  uint32_t Buffer<Derived, MemoryRegion>::size() const
  {
    return this->_size + this->_header;
  }

  template <typename Derived, typename MemoryRegion>
  uint32_t Buffer<Derived, MemoryRegion>::bytes() const
  {
    return this->_bytes;
  }

  #ifdef USE_LIBFABRIC // requires lkey refactor (up next)
  void *Buffer::lkey() const
  {
    assert(this->_mr);
    return fi_mr_desc(this->_mr);
  }
  #else
  template <typename Derived, typename MemoryRegion>
  uint32_t Buffer<Derived, MemoryRegion>::lkey() const
  {
    assert(this->_mr);
    // Apparently it's not needed and better to skip that check.
    return this->_mr->lkey;
    //return 0;
  }
  #endif

  #ifdef USE_LIBFABRIC
  uint64_t Buffer::rkey() const
  {
    assert(this->_mr);
    return fi_mr_key(this->_mr);
  }
  #else
  template <typename Derived, typename MemoryRegion>
  uint32_t Buffer<Derived, MemoryRegion>::rkey() const
  {
    assert(this->_mr);
    return this->_mr->rkey;
  }
  #endif

  template <typename Derived, typename MemoryRegion>
  uintptr_t Buffer<Derived, MemoryRegion>::address() const
  {
    assert(this->_mr);
    return reinterpret_cast<uint64_t>(this->_ptr);
  }

  template <typename Derived, typename MemoryRegion>
  void* Buffer<Derived, MemoryRegion>::ptr() const
  {
    return this->_ptr;
  }

  template <typename Derived, typename MemoryRegion>
  ScatterGatherElement<MemoryRegion> Buffer<Derived, MemoryRegion>::sge(uint32_t size, uint32_t offset) const
  {
    return {address() + offset, size, lkey()};
  }

}}

namespace rdmalib {

  template <typename MemoryRegion>
  ScatterGatherElement<MemoryRegion>::ScatterGatherElement()
  {
  }

  #ifdef USE_LIBFABRIC
  iovec *ScatterGatherElement::array() const
  {
    return _sges.data();
  }
  void **ScatterGatherElement::lkeys() const
  {
    return _lkeys.data();
  }
  #else
  template <typename MemoryRegion>
  ibv_sge * ScatterGatherElement<MemoryRegion>::array() const
  {
    return _sges.data();
  }
  #endif

  template <typename MemoryRegion>
  size_t ScatterGatherElement<MemoryRegion>::size() const
  {
    return _sges.size();
  }

  #ifdef USE_LIBFABRIC
  ScatterGatherElement::ScatterGatherElement(uint64_t addr, uint32_t bytes, void *lkey)
  {
    _sges.push_back({(void *)addr, bytes});
    _lkeys.push_back(lkey);
  }
  #else
  template <typename MemoryRegion>
  ScatterGatherElement<MemoryRegion>::ScatterGatherElement(uint64_t addr, uint32_t bytes, uint32_t lkey)
  {
    _sges.push_back({addr, bytes, lkey});
  }
  #endif

  RemoteBuffer::RemoteBuffer():
    addr(0),
    rkey(0),
    size(0)
  {}

  RemoteBuffer::RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size):
    addr(addr),
    rkey(rkey),
    size(size)
  {}

  #ifdef USE_LIBFABRIC
  RemoteBuffer::RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size):
    addr(addr),
    rkey(rkey),
    size(size)
  {}
  #else
    RemoteBuffer::RemoteBuffer(uintptr_t addr, uint32_t rkey, uint32_t size):
    addr(addr),
    rkey(rkey),
    size(size)
  {}
  #endif

}
