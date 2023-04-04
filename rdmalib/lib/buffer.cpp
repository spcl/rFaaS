
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

  Buffer::Buffer():
    _size(0),
    _header(0),
    _bytes(0),
    _byte_size(0),
    _ptr(nullptr),
    _mr(nullptr),
    _own_memory(false)
  {}

  Buffer::Buffer(Buffer && obj):
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

  Buffer & Buffer::operator=(Buffer && obj)
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

  Buffer::Buffer(uint32_t size, uint32_t byte_size, uint32_t header):
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

  Buffer::Buffer(void* ptr, uint32_t size, uint32_t byte_size):
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
  
  Buffer::~Buffer()
  {
    SPDLOG_DEBUG(
      "Deallocate {} bytes, mr {}, ptr {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr)
    );
    if(_mr)
      #ifdef USE_LIBFABRIC
      impl::expect_zero(fi_close(&_mr->fid));
      #else
      ibv_dereg_mr(_mr);
      #endif
    if(_own_memory)
      munmap(_ptr, _bytes);
  }

  #ifdef USE_LIBFABRIC
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
  void Buffer::register_memory(ibv_pd* pd, int access)
  {
    _mr = ibv_reg_mr(pd, _ptr, _bytes, access);
    impl::expect_nonnull(_mr);
    SPDLOG_DEBUG(
      "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey
    );
  }
  #endif

  #ifdef USE_LIBFABRIC
  fid_mr* Buffer::mr() const
  {
    return this->_mr;
  }
  #else
  ibv_mr* Buffer::mr() const
  {
    return this->_mr;
  }
  #endif

  uint32_t Buffer::data_size() const
  {
    return this->_size;
  }

  uint32_t Buffer::size() const
  {
    return this->_size + this->_header;
  }

  uint32_t Buffer::bytes() const
  {
    return this->_bytes;
  }

  #ifdef USE_LIBFABRIC
  void *Buffer::lkey() const
  {
    assert(this->_mr);
    return fi_mr_desc(this->_mr);
  }
  #else
  uint32_t Buffer::lkey() const
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
  uint32_t Buffer::rkey() const
  {
    assert(this->_mr);
    return this->_mr->rkey;
  }
  #endif

  uintptr_t Buffer::address() const
  {
    assert(this->_mr);
    return reinterpret_cast<uint64_t>(this->_ptr);
  }

  void* Buffer::ptr() const
  {
    return this->_ptr;
  }

  ScatterGatherElement Buffer::sge(uint32_t size, uint32_t offset) const
  {
    return {address() + offset, size, lkey()};
  }

}}

namespace rdmalib {

  ScatterGatherElement::ScatterGatherElement()
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
  ibv_sge * ScatterGatherElement::array() const
  {
    return _sges.data();
  }
  #endif

  size_t ScatterGatherElement::size() const
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
  ScatterGatherElement::ScatterGatherElement(uint64_t addr, uint32_t bytes, uint32_t lkey)
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
