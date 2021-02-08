
#include <cassert>
// inet_ntoa
#include <arpa/inet.h>
// traceback
#include <execinfo.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/format.h>

#include <rdmalib.hpp>

namespace {
  void traceback()
  {
    void* array[10];
    size_t size = backtrace(array, 10);
    char ** trace = backtrace_symbols(array, size);
    for(size_t i = 0; i < size; ++i)
      spdlog::warn("Traceback {}: {}", i, trace[i]);
    free(trace);
  }
}

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
    expect_nonnull(_mr);
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

namespace rdmalib {

  Address::Address(const std::string & ip, int port, bool passive)
  {
    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if(passive)
      hints.ai_flags = RAI_PASSIVE;
    expect_zero(rdma_getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &addrinfo));
    this->_port = port;
  }

  Address::~Address()
  {
    rdma_freeaddrinfo(addrinfo);
  }

  ConnectionConfiguration::ConnectionConfiguration()
  {
    memset(&attr, 0, sizeof(attr));
    memset(&conn_param, 0 , sizeof(conn_param));
  }

  Connection::Connection():
    id(nullptr),
    qp(nullptr)
  {}

  RDMAActive::RDMAActive(const std::string & ip, int port):
    _addr(ip, port, false),
    _ec(nullptr),
    _pd(nullptr),
    _req_count(0)
  {
    // Size of Queue Pair
    // Maximum requests in send queue
    _cfg.attr.cap.max_send_wr = 10;
    // Maximum requests in receive queue
    _cfg.attr.cap.max_recv_wr = 10;
    // Maximal number of scatter-gather requests in a work request in send queue
    _cfg.attr.cap.max_send_sge = 1;
    // Maximal number of scatter-gather requests in a work request in receive queue
    _cfg.attr.cap.max_recv_sge = 1;
    // Max inlined message size
    _cfg.attr.cap.max_inline_data = 56;
    // Reliable connection
    _cfg.attr.qp_type = IBV_QPT_RC;

    _cfg.conn_param.responder_resources = 5;
    _cfg.conn_param.initiator_depth =  5;
    _cfg.conn_param.retry_count = 3;  
    _cfg.conn_param.rnr_retry_count = 3; 
  }

  RDMAActive::~RDMAActive()
  {
    rdma_destroy_qp(this->_conn.id);
    ibv_dealloc_pd(this->_pd);
    rdma_destroy_ep(this->_conn.id);
    rdma_destroy_id(this->_conn.id);
  }

  void RDMAActive::allocate()
  {
    expect_zero(rdma_create_ep(&_conn.id, _addr.addrinfo, nullptr, nullptr));
    expect_zero(rdma_create_qp(_conn.id, _pd, &_cfg.attr));
    _pd = _conn.id->pd;
    _conn.qp = _conn.id->qp;
  }

  bool RDMAActive::connect()
  {
    if(rdma_connect(_conn.id, &_cfg.conn_param)) {
      spdlog::error("Connection unsuccesful, reason {} {}", errno, strerror(errno));
      return false;
    } else {
      spdlog::debug("Connection succesful to {}:{}", _addr._port, _addr._port);
    }
    return true;
  }

  ibv_qp* RDMAActive::qp() const
  {
    return this->_conn.qp;
  }

  ibv_pd* RDMAActive::pd() const
  {
    return this->_pd;
  }

  int32_t RDMAActive::post_recv(ScatterGatherElement && elem)
  {
    // FIXME: extend with multiple sges
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elem.array();
    wr.num_sge = elem.size();

    int ret = ibv_post_recv(_conn.qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post receive unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    return _req_count - 1;
  }

  int32_t RDMAActive::_post_write(ScatterGatherElement && elems, ibv_send_wr wr)
  {
    ibv_send_wr* bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.send_flags = IBV_SEND_SIGNALED;

    int ret = ibv_post_send(_conn.qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post write succesfull");
    return _req_count - 1;

  }

  int32_t RDMAActive::post_write(ScatterGatherElement && elems, uintptr_t addr, int rkey)
  {
    ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.wr.rdma.remote_addr = addr;
    wr.wr.rdma.rkey = rkey;
    return _post_write(std::forward<ScatterGatherElement>(elems), wr);
  }

  int32_t RDMAActive::post_write(ScatterGatherElement && elems, uintptr_t addr, int rkey, uint32_t immediate)
  {
    ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.imm_data = htonl(immediate);
    wr.wr.rdma.remote_addr = addr;
    wr.wr.rdma.rkey = rkey;
    return _post_write(std::forward<ScatterGatherElement>(elems), wr);
  }

  int32_t RDMAActive::post_atomics(ScatterGatherElement && elems, uintptr_t addr, int rkey, uint64_t add)
  {
    ibv_send_wr wr, *bad;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.atomic.remote_addr = addr;
    wr.wr.atomic.rkey = rkey;
    wr.wr.atomic.compare_add = add;

    int ret = ibv_post_send(_conn.qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post write succesfull");
    return _req_count - 1;
  }

  ibv_wc RDMAActive::poll_wc(QueueType type)
  {
    struct ibv_wc wc;
    while(ibv_poll_cq(type == QueueType::RECV ? _conn.qp->recv_cq : _conn.qp->send_cq, 1, &wc) == 0);
    spdlog::debug("Received WC {} Status {}", wc.wr_id, ibv_wc_status_str(wc.status));
    return wc;
  }

  RDMAPassive::RDMAPassive(const std::string & ip, int port):
    _addr(ip, port, true),
    _ec(nullptr),
    _listen_id(nullptr),
    _pd(nullptr),
    _req_count(0)
  {
    // Size of Queue Pair
    _cfg.attr.cap.max_send_wr = 10;
    _cfg.attr.cap.max_recv_wr = 10;
    _cfg.attr.cap.max_send_sge = 1;
    _cfg.attr.cap.max_recv_sge = 1;
    _cfg.attr.cap.max_inline_data = 56;
    _cfg.attr.qp_type = IBV_QPT_RC;

    _cfg.conn_param.responder_resources = 5;
    _cfg.conn_param.initiator_depth = 5;
    _cfg.conn_param.retry_count = 3; 
    _cfg.conn_param.rnr_retry_count = 3;  
  }

  RDMAPassive::~RDMAPassive()
  {
    for(auto & c : _connections) {
      rdma_destroy_qp(c.id);
      rdma_destroy_id(c.id);
    }
    ibv_dealloc_pd(this->_pd);
    rdma_destroy_ep(this->_listen_id);
    rdma_destroy_id(this->_listen_id);
  }

  void RDMAPassive::allocate()
  {
    // Start listening
    expect_zero(rdma_create_ep(&this->_listen_id, _addr.addrinfo, nullptr, nullptr));
    assert(!rdma_listen(this->_listen_id, 10));
    this->_addr._port = ntohs(rdma_get_src_port(this->_listen_id));
    this->_ec = this->_listen_id->channel;
    spdlog::info("Listening on port {}", this->_addr._port);

    // Alocate protection domain
    expect_nonnull(_pd = ibv_alloc_pd(_listen_id->verbs));
  }

  ibv_pd* RDMAPassive::pd() const
  {
    return this->_pd;
  }

  std::optional<Connection> RDMAPassive::poll_events()
  {
    rdma_cm_event* event;
    if(rdma_get_cm_event(this->_ec, &event)) {
      spdlog::error("Event poll unsuccesful, reason {} {}", errno, strerror(errno));
      return {};
    }
    if(event->event != RDMA_CM_EVENT_CONNECT_REQUEST){
      spdlog::error("Event {} is not RDMA_CM_EVENT_CONNECT_REQUEST", rdma_event_str(event->event));
      return {};
    }

    // Now receive id for the communication
    Connection connection;
    connection.id = event->id;
    // destroys event
    rdma_ack_cm_event(event);
    expect_zero(rdma_create_qp(connection.id, _pd, &_cfg.attr));
    if(rdma_accept(connection.id, &_cfg.conn_param)) {
      spdlog::error("Conection accept unsuccesful, reason {} {}", errno, strerror(errno));
      return {};
    }
    spdlog::debug("Accepted connection"); 
    connection.qp = connection.id->qp;

    _connections.push_back(connection);
    return std::optional<Connection>{connection};
  }

  int32_t RDMAPassive::post_send(const Connection & conn, ScatterGatherElement && elems)
  {
    struct ibv_send_wr wr, *bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;

    int ret = ibv_post_send(conn.qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post send unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post send succesfull");
    return _req_count - 1;
  }

  int32_t RDMAPassive::post_recv(const Connection & conn, ScatterGatherElement && elems)
  {
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();

    int ret = ibv_post_recv(conn.qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post receive unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Posted receive succesfull");
    return _req_count - 1;
  }

  ibv_wc RDMAPassive::poll_wc(const Connection & conn, QueueType type)
  {
    struct ibv_wc wc;
    while(ibv_poll_cq(type == QueueType::RECV ? conn.qp->recv_cq : conn.qp->send_cq, 1, &wc) == 0);
    spdlog::debug("Send WC {} Status {}", wc.wr_id, ibv_wc_status_str(wc.status));
    return wc;
  }

  ScatterGatherElement::ScatterGatherElement()
  {
  }

  ibv_sge * ScatterGatherElement::array()
  {
    return _sges.data();
  }

  size_t ScatterGatherElement::size()
  {
    return _sges.size();
  }
}
