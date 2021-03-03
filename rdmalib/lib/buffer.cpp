
// mmap
#include <sys/mman.h>
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib { namespace impl {

  Buffer::Buffer():
    _size(0),
    _header(0),
    _bytes(0),
    _mr(nullptr),
    _ptr(nullptr)
  {}

  Buffer::Buffer(Buffer && obj):
    _size(obj._size),
    _header(obj._header),
    _bytes(obj._bytes),
    _mr(obj._mr),
    _ptr(obj._ptr)
  {
    obj._size = obj._bytes = obj._header = 0;
    obj._ptr = obj._mr = nullptr;
  }

  Buffer & Buffer::operator=(Buffer && obj)
  {
    _size = obj._size;
    _bytes = obj._bytes;
    _header = obj._header;
    _ptr = obj._ptr;
    _mr = obj._mr;

    obj._size = obj._bytes = 0;
    obj._ptr = obj._mr = nullptr;
    return *this;
  }

  Buffer::Buffer(uint32_t size, uint32_t byte_size, uint32_t header):
    _size(size),
    _header(header),
    _bytes((size + header) * byte_size),
    _mr(nullptr)
  {
    //size_t alloc = _bytes;
    //if(alloc < 4096) {
    //  alloc = 4096;
    //  spdlog::warn("Page too small, allocating {} bytes", alloc);
    //}
    // page-aligned address for maximum performance
    _ptr = mmap(nullptr, _bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  }
  
  Buffer::~Buffer()
  {
    if(_mr)
      ibv_dereg_mr(_mr);
    munmap(_ptr, _bytes);
  }

  void Buffer::register_memory(ibv_pd* pd, int access)
  {
    _mr = ibv_reg_mr(pd, _ptr, _bytes, access);
    impl::expect_nonnull(_mr);
    SPDLOG_DEBUG(
      "Allocated {} bytes, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey
    );
  }

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

  uint32_t Buffer::lkey() const
  {
    assert(this->_mr);
    // Apparently it's not needed and better to skip that check.
    return this->_mr->lkey;
    //return 0;
  }

  uint32_t Buffer::rkey() const
  {
    assert(this->_mr);
    return this->_mr->rkey;
  }

  uintptr_t Buffer::address() const
  {
    assert(this->_mr);
    return reinterpret_cast<uint64_t>(this->_ptr);
  }

  void* Buffer::ptr() const
  {
    return this->_ptr;
  }

}}

namespace rdmalib {

  RemoteBuffer::RemoteBuffer():
    addr(0),
    rkey(0),
    size(0)
  {}

  RemoteBuffer::RemoteBuffer(uintptr_t addr, uint32_t rkey, uint32_t size):
    addr(addr),
    rkey(rkey),
    size(size)
  {}

}
