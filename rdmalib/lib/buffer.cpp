
// mmap
#include <sys/mman.h>
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib { namespace impl {

  Buffer::Buffer(size_t size, size_t byte_size):
    _size(size),
    _bytes(size * byte_size),
    _mr(nullptr)
  {
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
    spdlog::debug(
      "Allocated {} bytes, address {}, lkey {}, rkey {}",
      _bytes, fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey
    );
  }

  size_t Buffer::size() const
  {
    return this->_size;
  }

  uint32_t Buffer::lkey() const
  {
    assert(this->_mr);
    return this->_mr->lkey;
  }

  uint32_t Buffer::rkey() const
  {
    assert(this->_mr);
    return this->_mr->rkey;
  }

  uintptr_t Buffer::ptr() const
  {
    assert(this->_mr);
    return reinterpret_cast<uint64_t>(this->_ptr);
  }

}}
