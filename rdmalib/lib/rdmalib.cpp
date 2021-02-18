
#include <cassert>
// inet_ntoa
#include <arpa/inet.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/format.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib {

  Address::Address(const std::string & ip, int port, bool passive)
  {
    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if(passive)
      hints.ai_flags = RAI_PASSIVE;
    impl::expect_zero(rdma_getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &addrinfo));
    this->_port = port;
  }

  Address::~Address()
  {
    rdma_freeaddrinfo(addrinfo);
  }

  RDMAActive::RDMAActive(const std::string & ip, int port):
    _addr(ip, port, false),
    _ec(nullptr),
    _pd(nullptr)
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
    //ibv_dealloc_pd(this->_pd);
  }

  void RDMAActive::allocate()
  {
    impl::expect_zero(rdma_create_ep(&_conn._id, _addr.addrinfo, nullptr, nullptr));
    impl::expect_zero(rdma_create_qp(_conn._id, _pd, &_cfg.attr));
    _pd = _conn._id->pd;
    _conn._qp = _conn._id->qp;
  }

  bool RDMAActive::connect()
  {
    if(rdma_connect(_conn._id, &_cfg.conn_param)) {
      spdlog::error("Connection unsuccesful, reason {} {}", errno, strerror(errno));
      return false;
    } else {
      SPDLOG_DEBUG("Connection succesful to {}:{}", _addr._port, _addr._port);
    }
    return true;
  }

  ibv_pd* RDMAActive::pd() const
  {
    return this->_pd;
  }

  Connection & RDMAActive::connection()
  {
    return this->_conn;
  }

  RDMAPassive::RDMAPassive(const std::string & ip, int port):
    _addr(ip, port, true),
    _ec(nullptr),
    _listen_id(nullptr),
    _pd(nullptr)
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
    ibv_dealloc_pd(this->_pd);
    rdma_destroy_ep(this->_listen_id);
    rdma_destroy_id(this->_listen_id);
  }

  void RDMAPassive::allocate()
  {
    // Start listening
    impl::expect_zero(rdma_create_ep(&this->_listen_id, _addr.addrinfo, nullptr, nullptr));
    assert(!rdma_listen(this->_listen_id, 10));
    this->_addr._port = ntohs(rdma_get_src_port(this->_listen_id));
    this->_ec = this->_listen_id->channel;
    spdlog::info("Listening on port {}", this->_addr._port);

    // Alocate protection domain
    impl::expect_nonnull(_pd = ibv_alloc_pd(_listen_id->verbs));
  }

  ibv_pd* RDMAPassive::pd() const
  {
    return this->_pd;
  }

  Connection* RDMAPassive::poll_events(std::function<void(Connection&)> before_accept)
  {
    rdma_cm_event* event;
    if(rdma_get_cm_event(this->_ec, &event)) {
      spdlog::error("Event poll unsuccesful, reason {} {}", errno, strerror(errno));
      return nullptr;
    }
    if(event->event != RDMA_CM_EVENT_CONNECT_REQUEST){
      spdlog::error("Event {} is not RDMA_CM_EVENT_CONNECT_REQUEST", rdma_event_str(event->event));
      return nullptr;
    }

    // Now receive id for the communication
    Connection connection;
    connection._id = event->id;
    // destroys event
    rdma_ack_cm_event(event);
    impl::expect_zero(rdma_create_qp(connection._id, _pd, &_cfg.attr));
    connection._qp = connection._id->qp;
    if(before_accept)
      before_accept(connection);
    if(rdma_accept(connection._id, &_cfg.conn_param)) {
      spdlog::error("Conection accept unsuccesful, reason {} {}", errno, strerror(errno));
      return nullptr;
    }
    SPDLOG_DEBUG("Accepted connection"); 

    _connections.push_back(std::move(connection));
    return &_connections.back();
  }
}
