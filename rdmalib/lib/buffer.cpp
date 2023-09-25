
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

namespace rdmalib
{
  namespace impl
  {
    LibfabricBuffer::~LibfabricBuffer()
    {
      SPDLOG_DEBUG(
          "Deallocate {} bytes, mr {}, ptr {}",
          _bytes, fmt::ptr(_mr), fmt::ptr(_ptr));
      if (_mr)
        impl::expect_zero(fi_close(&_mr->fid));
      if (_own_memory)
        munmap(_ptr, _bytes);
    }

    VerbsBuffer::~VerbsBuffer()
    {
      SPDLOG_DEBUG(
          "Deallocate {} bytes, mr {}, ptr {}",
          _bytes, fmt::ptr(_mr), fmt::ptr(_ptr));
      if (_mr)
        ibv_dereg_mr(_mr);
      if (_own_memory)
        munmap(_ptr, _bytes);
    }

    void LibfabricBuffer::register_memory(LibfabricBuffer::pd_t pd, int access)
    {
      int ret = fi_mr_reg(pd, _ptr, _bytes, access, 0, 0, 0, &_mr, nullptr);
      impl::expect_zero(ret);
      SPDLOG_DEBUG(
          "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
          _bytes, fmt::ptr(_mr), fmt::ptr(_ptr), fmt::ptr(fi_mr_desc(_mr)), fi_mr_key(_mr));
    }

    void VerbsBuffer::register_memory(VerbsBuffer::pd_t pd, int access)
    {
      _mr = ibv_reg_mr(pd, _ptr, _bytes, access);
      impl::expect_nonnull(_mr);
      SPDLOG_DEBUG(
          "Registered {} bytes, mr {}, address {}, lkey {}, rkey {}",
          _bytes, fmt::ptr(_mr), fmt::ptr(_mr->addr), _mr->lkey, _mr->rkey);
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
      // return 0;
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

  }
}

namespace rdmalib
{
}