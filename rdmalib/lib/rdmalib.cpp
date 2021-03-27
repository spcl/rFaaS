
#include "rdmalib/connection.hpp"
#include <cassert>
// inet_ntoa
#include <arpa/inet.h>
#include <netdb.h>

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

  Address::Address(const std::string & sip,  const std::string & dip, int port)
  {
    struct sockaddr_in server_in, local_in;
    memset(&server_in, 0, sizeof(server_in));
    memset(&local_in, 0, sizeof(local_in));

    /*address of remote node*/
    server_in.sin_family = AF_INET;
    server_in.sin_port = htons(port);  
    inet_pton(AF_INET, dip.c_str(),   &server_in.sin_addr);

    /*address of local device*/
    local_in.sin_family = AF_INET; 
    inet_pton(AF_INET, sip.c_str(), &local_in.sin_addr);

    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    hints.ai_src_len = sizeof(local_in);
    hints.ai_dst_len = sizeof(server_in);
    hints.ai_src_addr = (struct sockaddr *)(&local_in);
    hints.ai_dst_addr = (struct sockaddr *)(&server_in);

    impl::expect_zero(rdma_getaddrinfo(NULL, NULL, &hints, &addrinfo));
    this->_port = port;
  }


  Address::~Address()
  {
    rdma_freeaddrinfo(addrinfo);
  }

  RDMAActive::RDMAActive(const std::string & ip, int port, int recv_buf, int max_inline_data):
    _conn(nullptr),
    _addr(ip, port, false),
    _ec(nullptr),
    _pd(nullptr)
  {
    // Size of Queue Pair
    // Maximum requests in send queue
    _cfg.attr.cap.max_send_wr = 20;
    // Maximum requests in receive queue
    _cfg.attr.cap.max_recv_wr = recv_buf;
    // Maximal number of scatter-gather requests in a work request in send queue
    _cfg.attr.cap.max_send_sge = 5;
    // Maximal number of scatter-gather requests in a work request in receive queue
    _cfg.attr.cap.max_recv_sge = 5;
    // Max inlined message size
    _cfg.attr.cap.max_inline_data = max_inline_data;
    // Reliable connection
    _cfg.attr.qp_type = IBV_QPT_RC;
    _cfg.attr.sq_sig_all = 1;

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
    if(!_conn) {
      _conn = std::unique_ptr<Connection>(new Connection());
      SPDLOG_DEBUG("Allocate new connection");
      impl::expect_zero(rdma_create_ep(&_conn->_id, _addr.addrinfo, nullptr, nullptr));
      impl::expect_zero(rdma_create_qp(_conn->_id, _pd, &_cfg.attr));
      _pd = _conn->_id->pd;
      _conn->_qp = _conn->_id->qp;
    }

    // An attempt to bind the active client to a specifi device.
    //struct addrinfo *addr;
    //getaddrinfo("192.168.0.12", "0", nullptr, &addr);
    //rdma_cm_id *conn = nullptr;
    //auto ec = rdma_create_event_channel();
    //int ret = rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP);
    //spdlog::info("{} {} {} {}", ret, conn != nullptr, addr->ai_addr != nullptr, _addr.addrinfo->ai_dst_addr != nullptr);
    ////ret = rdma_resolve_addr(conn, addr->ai_addr, _addr.addrinfo->ai_dst_addr, 2000);
    ////spdlog::info("{} {} {} {} {}", ret, conn != nullptr, conn->verbs != nullptr, conn->pd != nullptr, conn->qp != nullptr);

    //ret = rdma_bind_addr(conn, addr->ai_addr);
    //spdlog::info("{} {} {} {} {} {}", ret, errno, conn != nullptr, conn->verbs != nullptr, conn->pd != nullptr, conn->qp != nullptr);

    //int num_of_device;
    //struct ibv_device **dev_list;
    //struct ibv_device *ib_dev = NULL;

    //struct addrinfo *addr2;
    //ret = rdma_resolve_addr(conn, nullptr, _addr.addrinfo->ai_dst_addr, 2000);
    //spdlog::info("{} {} {} {} {}", ret, conn != nullptr, conn->verbs != nullptr, conn->pd != nullptr, conn->qp != nullptr);
    ////dev_list = ibv_get_device_list(&num_of_device);
    ////spdlog::info("{}", ibv_get_device_name(dev_list[1]));
    ////conn->verbs = ibv_open_device(dev_list[1]);
    //impl::expect_zero(rdma_create_qp(conn, conn->pd, &_cfg.attr));
    //_conn._id = conn;
    ////impl::expect_zero(rdma_create_ep(&_conn._id, _addr.addrinfo, nullptr, nullptr));
    ////impl::expect_zero(rdma_create_qp(_conn._id, _pd, &_cfg.attr));
    //struct rdma_cm_event *event;
    //while (1) {
    //  ret = rdma_get_cm_event(_conn._id->channel, &event);
    //  if (ret) { 
    //    exit(ret);
    //  }
    //  spdlog::info("{} {}", event->event, RDMA_CM_EVENT_ADDR_ERROR);
    //  rdma_ack_cm_event(event);
    //  break;
    //}
    ////
    ////
    //ret = rdma_resolve_route(_conn._id, 2000);
    //spdlog::info("{} {} {} {} {} {}", ret, errno, conn != nullptr, conn->verbs != nullptr, conn->pd != nullptr, conn->qp != nullptr);
  }

  bool RDMAActive::connect()
  {
    allocate();
    if(rdma_connect(_conn->_id, &_cfg.conn_param)) {
      spdlog::error("Connection unsuccesful, reason {} {}", errno, strerror(errno));
      return false;
    } else {
      spdlog::info("Connection succesful to {}:{}, on device {}", _addr._port, _addr._port, ibv_get_device_name(this->_conn->_id->verbs->device));
    }
    return true;
  }

  void RDMAActive::disconnect()
  {
    SPDLOG_DEBUG("Disconnecting");
    impl::expect_zero(rdma_disconnect(_conn->_id));
    _conn.reset();
    _pd = nullptr;
    //rdma_destroy_qp(_conn._id);
    //_conn._qp = nullptr;
  }

  ibv_pd* RDMAActive::pd() const
  {
    return this->_pd;
  }

  Connection & RDMAActive::connection()
  {
    return *this->_conn;
  }

  RDMAPassive::RDMAPassive(const std::string & ip, int port, int recv_buf, bool initialize, int max_inline_data):
    _addr(ip, port, true),
    _ec(nullptr),
    _listen_id(nullptr),
    _pd(nullptr)
  {
    // Size of Queue Pair
    _cfg.attr.cap.max_send_wr = 20;
    _cfg.attr.cap.max_recv_wr = recv_buf;
    _cfg.attr.cap.max_send_sge = 5;
    _cfg.attr.cap.max_recv_sge = 5;
    _cfg.attr.cap.max_inline_data = max_inline_data;
    _cfg.attr.qp_type = IBV_QPT_RC;
    _cfg.attr.sq_sig_all = 1;

    _cfg.conn_param.responder_resources = 5;
    _cfg.conn_param.initiator_depth = 5;
    _cfg.conn_param.retry_count = 3; 
    _cfg.conn_param.rnr_retry_count = 3;

    _connections.reserve(MAX_NUMBER_CONNECTIONS);
    if(initialize)
      this->allocate();
  }

  RDMAPassive::~RDMAPassive()
  {
    SPDLOG_DEBUG("Destroy");
    ibv_dealloc_pd(this->_pd);
    rdma_destroy_ep(this->_listen_id);
    rdma_destroy_id(this->_listen_id);
  }

  void RDMAPassive::allocate()
  {
    // Start listening
    impl::expect_zero(rdma_create_ep(&this->_listen_id, _addr.addrinfo, nullptr, nullptr));
    impl::expect_zero(rdma_listen(this->_listen_id, 0));
    this->_addr._port = ntohs(rdma_get_src_port(this->_listen_id));
    this->_ec = this->_listen_id->channel;
    spdlog::info("Listening on device {}, port {}", ibv_get_device_name(this->_listen_id->verbs->device), this->_addr._port);

    // Alocate protection domain
    _pd = _listen_id->pd;
    //impl::expect_nonnull(_pd = ibv_alloc_pd(_listen_id->verbs));
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
    SPDLOG_DEBUG("CREATE QP {} {} {}", fmt::ptr(connection._id), fmt::ptr(_pd), fmt::ptr(this->_listen_id->pd));
     _cfg.attr.send_cq = NULL;
     _cfg.attr.recv_cq = NULL;
    SPDLOG_DEBUG("used cq for creating a qp {} {}", fmt::ptr(_cfg.attr.send_cq),fmt::ptr(_cfg.attr.recv_cq));
    impl::expect_zero(rdma_create_qp(connection._id, _pd, &_cfg.attr));
    SPDLOG_DEBUG("CREATE QP with qpn {}", connection._id->qp->qp_num);
    connection._qp = connection._id->qp;
    connection.initialize();
    _connections.push_back(std::move(connection));

    rdmalib::Connection & conn = _connections.back();
    if(before_accept)
      before_accept(conn);
    if(rdma_accept(conn._id, &_cfg.conn_param)) {
      spdlog::error("Conection accept unsuccesful, reason {} {}", errno, strerror(errno));
      return nullptr;
    }
    SPDLOG_DEBUG("Accepted connection"); 

    return &conn;
  }
}
