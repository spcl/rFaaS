
// mmap
#include <sys/mman.h>

// #ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
// #else
#include <infiniband/verbs.h>
// #endif

#include <rdmalib/libraries.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib { namespace impl {

  template <typename Derived, typename Library>
  Buffer<Derived, Library>::Buffer():
    _size(0),
    _header(0),
    _bytes(0),
    _byte_size(0),
    _ptr(nullptr),
    _mr(nullptr),
    _own_memory(false)
  {}

  template <typename Derived, typename Library>
  Buffer<Derived, Library>::Buffer(Buffer<Derived, Library> && obj):
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

  template <typename Derived, typename Library>
  Buffer<Derived, Library> & Buffer<Derived, Library>::operator=(Buffer<Derived, Library> && obj)
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

  template <typename Derived, typename Library>
  Buffer<Derived, Library>::Buffer(uint32_t size, uint32_t byte_size, uint32_t header):
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

  template <typename Derived, typename Library>
  Buffer<Derived, Library>::Buffer(void* ptr, uint32_t size, uint32_t byte_size):
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
  
  void LibfabricBuffer::destroy()
  {
    SPDLOG_DEBUG(
      "Deallocate {} bytes, mr {}, ptr {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr)
    );
    if(_mr)
      impl::expect_zero(fi_close(&_mr->fid));
    if(_own_memory)
      munmap(_ptr, _bytes);
  }

  void VerbsBuffer::destroy()
  {
    SPDLOG_DEBUG(
      "Deallocate {} bytes, mr {}, ptr {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr)
    );
    if(_mr)
      ibv_dereg_mr(_mr);
    if(_own_memory)
      munmap(_ptr, _bytes);
  }

  void LibfabricBuffer::register_memory(LibfabricBuffer::pd_t pd, int access)
  {
    int ret = fi_mr_reg(pd, _ptr, _bytes, access, 0, 0, 0, &_mr, nullptr);
    impl::expect_zero(ret);
    SPDLOG_DEBUG(
      "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_ptr), fmt::ptr(fi_mr_desc(_mr)), fi_mr_key(_mr)
    );
  }

  void VerbsBuffer::register_memory(VerbsBuffer::pd_t pd, int access)
  {
    _mr = ibv_reg_mr(pd, _ptr, _bytes, access);
    impl::expect_nonnull(_mr);
    SPDLOG_DEBUG(
      "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr), fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey
    );
  }

  template <typename Derived, typename Library>
  uint32_t Buffer<Derived, Library>::data_size() const
  {
    return this->_size;
  }

  template <typename Derived, typename Library>
  uint32_t Buffer<Derived, Library>::size() const
  {
    return this->_size + this->_header;
  }

  template <typename Derived, typename Library>
  uint32_t Buffer<Derived, Library>::bytes() const
  {
    return this->_bytes;
  }

  LibfabricBuffer::lkey_t LibfabricBuffer::lkey() const
  {
    assert(this->_mr);
    return fi_mr_desc(this->_mr);
  }

  VerbsBuffer::lkey_t VerbsBuffer::lkey() const
  {
    assert(this->_mr);
    // Apparently it's not needed and better to skip that check.
    return this->_mr->lkey;
    //return 0;
  }

  LibfabricBuffer::rkey_t LibfabricBuffer::rkey() const
  {
    assert(this->_mr);
    return fi_mr_key(this->_mr);
  }
  VerbsBuffer::rkey_t VerbsBuffer::rkey() const
  {
    assert(this->_mr);
    return this->_mr->rkey;
  }

  template <typename Derived, typename Library>
  uintptr_t Buffer<Derived, Library>::address() const
  {
    assert(this->_mr);
    return reinterpret_cast<uint64_t>(this->_ptr);
  }

  template <typename Derived, typename Library>
  void* Buffer<Derived, Library>::ptr() const
  {
    return this->_ptr;
  }

  template <typename Derived, typename Library>
  ScatterGatherElement<Derived, Library> Buffer<Derived, Library>::sge(uint32_t size, uint32_t offset) const
  {
    return {address() + offset, size, lkey()};
  }

}}

namespace rdmalib {}