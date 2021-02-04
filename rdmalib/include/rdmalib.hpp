
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <optional>

// mmap
#include <sys/mman.h>

#include <rdma/rdma_cma.h>

namespace {

  void traceback();

  template<typename U>
  void expect_zero(U && u)
  {
    if(u) {
      spdlog::error("Expected zero, found: {}", u);
      traceback();
    }
    assert(!u);
  }

  template<typename U>
  void expect_nonzero(U && u)
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", u);
      traceback();
    }
    assert(u);
  }

  template<typename U>
  void expect_nonnull(U* ptr)
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      traceback();
    }
    assert(ptr);
  }

}

namespace rdmalib {

  namespace impl {

    // move non-template methods from header
    struct Buffer {
    protected:
      size_t _size;
      size_t _bytes;
      ibv_mr* _mr;
      void* _ptr;

      Buffer(size_t size, size_t byte_size);
      ~Buffer();
    public:
      uintptr_t ptr() const;
      size_t size() const;
      void register_memory(ibv_pd *pd, int access);
      uint32_t lkey() const;
      uint32_t rkey() const;
    };

  }

  template<typename T>
  struct Buffer : impl::Buffer{

    Buffer(size_t size):
      impl::Buffer(size, sizeof(T))
    {}

    T* data() const
    {
      return static_cast<T*>(this->_ptr);
    }
  };

  // Implemented as IPV4
  struct Address {
    rdma_addrinfo *addrinfo;
    rdma_addrinfo hints;
    uint16_t _port;

    Address(const std::string & ip, int port, bool passive);
    ~Address();
  };

  struct ConnectionConfiguration {
    // Configuration of QP
    ibv_qp_init_attr attr;
    rdma_conn_param conn_param;

    ConnectionConfiguration();
  };

  // State of a communication:
  // a) communication ID
  // b) Queue Pair
  struct Connection {
    rdma_cm_id* id;
    ibv_qp* qp; 

    Connection();
  };

  struct ScatterGatherElement {
    ibv_sge sge;

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf)
    {
      sge.addr = buf.ptr();
      sge.length = buf.size();
      sge.lkey = buf.lkey();
    }
  };

  struct RDMAActive {
    ConnectionConfiguration _cfg;
    Connection _conn;
    Address _addr;
    rdma_event_channel * _ec;
    ibv_pd* _pd;
    int32_t _req_count;

    RDMAActive(const std::string & ip, int port);
    ~RDMAActive();
    void allocate();
    bool connect();
    ibv_qp* qp() const;
    ibv_pd* pd() const;

    int32_t post_recv(ScatterGatherElement && elem);
    // Blocking, no timeout
    ibv_wc poll_wc();
  };

  struct RDMAPassive {
    ConnectionConfiguration _cfg;
    Address _addr;
    rdma_event_channel * _ec;
    rdma_cm_id* _listen_id;
    ibv_pd* _pd;
    std::vector<Connection> _connections;

    RDMAPassive(const std::string & ip, int port);
    ~RDMAPassive();
    void allocate();
    ibv_pd* pd() const;
    std::optional<Connection> poll_events();

    int32_t post_send(const Connection & conn, ScatterGatherElement && elem);
    ibv_wc poll_wc(const Connection & conn);
  };

  //struct RDMAState {
  //  rdma_event_channel * _ec;
  //  rdma_cm_id * _id;

  //  RDMAState();
  //  ~RDMAState();

  //  RDMAListen listen(int port = 0) const;
  //  RDMAConnect connect() const;
  //};
}
