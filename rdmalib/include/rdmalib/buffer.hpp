
#ifndef __RDMALIB_BUFFER_HPP__
#define __RDMALIB_BUFFER_HPP__

#include <cstdint>
#include <utility>

#include <cereal/cereal.hpp>

// #ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <sys/uio.h>
// #else
struct ibv_pd;
struct ibv_mr;
struct ibv_sge;
// #endif

namespace rdmalib
{

  template <typename Library>
  struct ScatterGatherElement;

  struct ibverbs;
  struct libfabric;

  template <typename Library>
  struct library_traits;

  template <>
  struct library_traits<ibverbs>
  {
    //using type = ibverbs;
    typedef ibv_mr *mr_t;
    typedef ibv_pd *pd_t;
    typedef uint32_t lkey_t;
    typedef uint32_t rkey_t;
    typedef iovec sge_t;
  };

  template <>
  struct library_traits<libfabric>
  {
    //using type = libfabric;
    typedef fid_mr *mr_t;
    typedef fid_domain *pd_t;
    typedef void *lkey_t;
    typedef uint64_t rkey_t;
    typedef ibv_sge sge_t;
  };

  namespace impl
  {

    // move non-template methods from header
    template <typename Derived>
    struct Buffer
    {
    protected:
      using Library = typename Derived::library;
      using mr_t   = typename library_traits<Library>::mr_t;
      using pd_t   = typename library_traits<Library>::pd_t;  
      using lkey_t = typename library_traits<Library>::lkey_t;
      using rkey_t = typename library_traits<Library>::rkey_t;

      /*
      typedef typename Derived::library                  Library;
      typedef typename library_traits<Library>::mr_t    mr_t   ;
      typedef typename library_traits<Library>::pd_t    pd_t   ;
      typedef typename library_traits<Library>::lkey_t  lkey_t ;
      typedef typename library_traits<Library>::rkey_t  rkey_t ;
      */

      uint32_t _size;
      uint32_t _header;
      uint32_t _bytes;
      uint32_t _byte_size;
      void *_ptr;
      mr_t _mr;
      bool _own_memory;

      Buffer();
      Buffer(void *ptr, uint32_t size, uint32_t byte_size);
      Buffer(uint32_t size, uint32_t byte_size, uint32_t header);
      Buffer(Buffer &&);
      Buffer &operator=(Buffer &&obj);
      ~Buffer()
      {
        static_cast<Derived *>(this)->destroy();
      }

    public:
      uintptr_t address() const;
      void *ptr() const;
      mr_t mr() const;
      uint32_t data_size() const;
      uint32_t size() const;
      uint32_t bytes() const;
      void register_memory(pd_t pd, int access)
      {
        static_cast<Derived*>(this)->register_memory(pd, access);
      }
      lkey_t lkey() const
      {
        static_cast<Derived*>(this)->lkey();
      }
      rkey_t rkey() const
      {
        static_cast<Derived*>(this)->rkey();
      }
      ScatterGatherElement<Library> sge(uint32_t size, uint32_t offset) const;
    };

    struct LibfabricBuffer : Buffer<LibfabricBuffer>
    {
      //using library = libfabric;
      void register_memory(pd_t pd, int access);
      lkey_t lkey() const;
      rkey_t rkey() const;
      void destroy();
    };

    struct VerbsBuffer : Buffer<VerbsBuffer>
    {
      //using library = ibverbs;
      void register_memory(pd_t pd, int access);
      lkey_t lkey() const;
      rkey_t rkey() const;
      void destroy();
    };

  }

  template <typename Library>
  struct RemoteBuffer
  {

    using rkey_t = typename library_traits<Library>::rkey_t;

    uintptr_t addr;
    uint64_t rkey;
    uint32_t size;

    RemoteBuffer();
    // When accessing the remote buffer, we might not need to know the size.
    RemoteBuffer(uintptr_t addr, uint64_t rkey, uint32_t size = 0);

    RemoteBuffer(uintptr_t addr, rkey_t rkey, uint32_t size);

    template <class Archive>
    void serialize(Archive &ar)
    {
      ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
    }
  };

  template <typename T, typename Library>
  struct Buffer : impl::Buffer<Buffer<T, Library>>
  {
    using ImplBuffer = impl::Buffer<Buffer<T, Library>>;

    Buffer() : ImplBuffer()
    {
    }

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(T *ptr, uint32_t size) : ImplBuffer(ptr, size, sizeof(T))
    {
    }

    // Provide a buffer instance for existing memory pool
    // Does NOT free the associated resource
    Buffer(void *ptr, uint32_t size) : ImplBuffer(ptr, size, sizeof(T))
    {
    }

    Buffer(size_t size, size_t header = 0) : ImplBuffer(size, sizeof(T), header)
    {
    }

    Buffer<T, Library> &operator=(Buffer<T, Library> &&obj)
    {
      ImplBuffer::operator=(std::move(obj));
      return *this;
    }

    Buffer(const Buffer<T, Library> &obj) = delete;
    Buffer(Buffer<T, Library> &&obj) = default;

    T *data() const
    {
      // void pointer arithmetic is not allowed
      return reinterpret_cast<T *>(static_cast<char *>(this->_ptr) + this->_header);
    }
  };

  template <typename Derived>
  struct ScatterGatherElement
  {
    using Library = Derived::library;
    using sge_t  = typename library_traits<Library>::sge_t;
    using lkey_t = typename library_traits<Library>::lkey_t;

    mutable std::vector<sge_t> _sges;

    ScatterGatherElement();

    ScatterGatherElement(uint64_t addr, uint32_t bytes, lkey_t lkey);

    template <typename T>
    ScatterGatherElement(const Buffer<T, Library> &buf)
    {
      add(buf);
    }

    template <typename T>
    void add(const Buffer<T, Library> &buf);

    template <typename T>
    void add(const Buffer<T, Library> &buf, uint32_t size, size_t offset = 0);

    sge_t *array() const;

    size_t size() const;
  };

  struct VerbsScatterGatherElement : ScatterGatherElement<VerbsScatterGatherElement>
  {
    template <typename T>
    void add(const Buffer<T, Library> &buf)
    {
      // emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), buf.bytes(), buf.lkey()});
    }

    template <typename T>
    void add(const Buffer<T, Library> &buf, uint32_t size, size_t offset = 0)
    {
      // emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address() + offset, size, buf.lkey()});
    }

    sge_t *array() const;
    size_t size() const;

  };

  struct LibfabricScatterGatherElement : ScatterGatherElement<LibfabricScatterGatherElement>
  {
    mutable std::vector<lkey_t> _lkeys;

    template <typename T>
    void add(const Buffer<T, Library> &buf)
    {
      _sges.push_back({(void *)buf.address(), (size_t)buf.bytes()});
      _lkeys.push_back(buf.lkey());
    }

    template <typename T>
    void add(const Buffer<T, Library> &buf, uint32_t size, size_t offset = 0)
    {
      _sges.push_back({(void *)(buf.address() + offset), (size_t)size});
      _lkeys.push_back(buf.lkey());
    }

    sge_t *array() const;
    lkey_t *lkeys() const;
    size_t size() const;
  };
}

#endif
