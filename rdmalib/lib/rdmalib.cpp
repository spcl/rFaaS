
#include "rdmalib/connection.hpp"
#include "rdmalib/queue.hpp"
#include <cassert>
// inet_ntoa
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <netdb.h>

// poll on file descriptors
#include <poll.h>
#include <fcntl.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/format.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/util.hpp>
#include <stdexcept>

namespace rdmalib {

  Address::Address()
  {
    memset(&hints, 0, sizeof(hints));
    addrinfo = nullptr;
    this->_port = -1;
  }

  Address::Address(const std::string & ip, int port, bool passive)
  {
    impl::expect_false(ip.empty(), false, "Expected non-empty IP address!");

    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if(passive)
      hints.ai_flags = RAI_PASSIVE;

    impl::expect_zero(rdma_getaddrinfo(const_cast<char *>(ip.c_str()), const_cast<char *>(std::to_string(port).c_str()), &hints, &addrinfo));
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

  Address::Address(Address && obj):
    addrinfo(obj.addrinfo),
    hints(obj.hints),
    _port(obj._port)
  {
    obj.addrinfo = nullptr;
  }

  Address& Address::operator=(Address && obj)
  {
    hints = obj.hints;
    _port = obj._port;
    addrinfo = obj.addrinfo;

    obj.addrinfo = nullptr;

    return *this;
  }

  Address::~Address()
  {
    if(addrinfo)
      rdma_freeaddrinfo(addrinfo);
  }

  void Address::set_port(uint32_t port)
  {
    this->_port = port;
  }

  uint32_t Address::port() const
  {
    return _port;
  }

  RDMAActive::RDMAActive():
    _conn(nullptr),
    _ec(nullptr),
    _pd(nullptr),
    _is_connected(false)
  {}


  RDMAActive::RDMAActive(const std::string & ip, int port, int recv_buf, int max_inline_data):
    _conn(nullptr),
    _addr(ip, port, false),
    _ec(nullptr),
    _pd(nullptr),
    _recv_buf(recv_buf),
    _is_connected(false)
  {
    // Size of Queue Pair
    // Maximum requests in send queue
    // FIXME: configurable -> parallel workers
    _cfg.attr.cap.max_send_wr = 40;
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

    // FIXME: make dependent on the number of parallel workers
    _cfg.conn_param.responder_resources = 4;
    _cfg.conn_param.initiator_depth =  4;
    _cfg.conn_param.retry_count = 3;
    _cfg.conn_param.rnr_retry_count = 3;
    SPDLOG_DEBUG("Create RDMAActive");
  }

  RDMAActive& RDMAActive::operator=(RDMAActive && obj)
  {
    _conn = std::move(obj._conn);
    _addr = std::move(obj._addr);
    _ec = std::move(obj._ec);
    _pd = std::move(obj._pd);
    _cfg = std::move(obj._cfg);
    _recv_buf = std::move(obj._recv_buf);
    _is_connected = std::move(obj._is_connected);

    return *this;
  }

  RDMAActive::~RDMAActive()
  {
    //ibv_dealloc_pd(this->_pd);
    SPDLOG_DEBUG("Destroy RDMAActive");
  }

  void RDMAActive::allocate()
  {
    if(!_conn) {
      _conn = std::unique_ptr<Connection>(new Connection(this->_recv_buf));
      rdma_cm_id* id;
      impl::expect_zero(rdma_create_ep(&id, _addr.addrinfo, nullptr, nullptr));
      impl::expect_zero(rdma_create_qp(id, _pd, &_cfg.attr));
      _conn->initialize(id);
      _pd = _conn->id()->pd;
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

  bool RDMAActive::connect(uint32_t secret)
  {
    allocate();
    if(secret) {
      _cfg.conn_param.private_data = &secret;
      _cfg.conn_param.private_data_len = sizeof(uint32_t);
      SPDLOG_DEBUG("Setting connection secret {} of length {}", secret, sizeof(uint32_t));
    }
    if(rdma_connect(_conn->id(), &_cfg.conn_param)) {
      spdlog::error("Connection unsuccesful, reason {} {}", errno, strerror(errno));
      _conn.reset();
      _pd = nullptr;
      return false;
    } else {
      spdlog::debug(
        "[RDMAActive] Connection succesful to {}:{}, on device {}",
        _addr._port, _addr._port, ibv_get_device_name(this->_conn->id()->verbs->device)
      );
    }

    _is_connected = true;

    //struct ibv_qp_attr attr;
    //struct ibv_qp_init_attr init_attr;
    //impl::expect_zero(ibv_query_qp(_conn->_qp, &attr, IBV_QP_DEST_QPN, &init_attr ));
    //SPDLOG_DEBUG("Local QPN {}, remote QPN {} ",_conn->_qp->qp_num, attr.dest_qp_num);

    return true;
  }

  void RDMAActive::disconnect()
  {
    spdlog::debug("[RDMAActive] Disonnecting connection with id {}", fmt::ptr(_conn->id()));
    impl::expect_zero(rdma_disconnect(_conn->id()));
    _conn.reset();
    _pd = nullptr;
    _is_connected = false;
  }

  ibv_pd* RDMAActive::pd() const
  {
    return this->_pd;
  }

  Connection & RDMAActive::connection()
  {
    return *this->_conn;
  }

  bool RDMAActive::is_connected() const
  {
    return _is_connected;
  }

  RDMAPassive::RDMAPassive(const std::string & ip, int port, int recv_buf, bool initialize, int max_inline_data):
    _addr(ip, port, true),
    _ec(nullptr),
    _listen_id(nullptr),
    _pd(nullptr),
    _recv_buf(recv_buf)
  {
    // Size of Queue Pair
    // FIXME: configurable -> parallel workers
    _cfg.attr.cap.max_send_wr = 40;
    _cfg.attr.cap.max_recv_wr = recv_buf;
    _cfg.attr.cap.max_send_sge = 5;
    _cfg.attr.cap.max_recv_sge = 5;
    _cfg.attr.cap.max_inline_data = max_inline_data;
    _cfg.attr.qp_type = IBV_QPT_RC;
    _cfg.attr.sq_sig_all = 1;

    // FIXME: make dependent on the number of parallel workers
    _cfg.conn_param.responder_resources = 4;
    _cfg.conn_param.initiator_depth = 4;
    _cfg.conn_param.retry_count = 3; 
    _cfg.conn_param.rnr_retry_count = 3;

    if(initialize)
      this->allocate();
  }

  RDMAPassive::~RDMAPassive()
  {
    if(this->_listen_id) {
      rdma_destroy_id(this->_listen_id);
    }

    if(this->_ec) {
      rdma_destroy_event_channel(this->_ec);
    }

    for(auto & [key, value] : _shared_recv_completions) {
      ibv_destroy_cq(std::get<1>(value));
      ibv_destroy_comp_channel(std::get<0>(value));
    }
  }

  RDMAPassive::RDMAPassive(RDMAPassive && obj):
    _cfg(std::move(obj._cfg)),
    _addr(std::move(obj._addr)),
    _ec(std::move(obj._ec)),
    _listen_id(std::move(obj._listen_id)),
    _pd(std::move(obj._pd)),
    _recv_buf(obj._recv_buf),
    _active_connections(std::move(obj._active_connections)),
    _shared_recv_completions(std::move(obj._shared_recv_completions))
  {
    obj._ec = nullptr;
    obj._listen_id = nullptr;
    obj._pd = nullptr;
  }

  RDMAPassive& RDMAPassive::operator=(RDMAPassive && obj)
  {
    _cfg = std::move(obj._cfg);
    _addr = std::move(obj._addr);
    _ec = std::move(obj._ec);
    _listen_id = std::move(obj._listen_id);
    _pd = std::move(obj._pd);
    _active_connections = std::move(obj._active_connections);
    _shared_recv_completions = std::move(obj._shared_recv_completions);

    obj._ec = nullptr;
    obj._listen_id = nullptr;
    obj._pd = nullptr;

    return *this;
  }

  void RDMAPassive::allocate()
  {
    // Start listening
    impl::expect_nonzero(this->_ec = rdma_create_event_channel());
    impl::expect_zero(rdma_create_id(this->_ec, &this->_listen_id, NULL, RDMA_PS_TCP));
    impl::expect_zero(rdma_bind_addr(this->_listen_id, this->_addr.addrinfo->ai_src_addr));
    impl::expect_zero(rdma_listen(this->_listen_id, 10));

    _addr.set_port(ntohs(rdma_get_src_port(this->_listen_id)));
    this->_pd = _listen_id->pd;

    spdlog::info(
      "Listening on device {}, port {}",
      ibv_get_device_name(this->_listen_id->verbs->device), this->_addr._port
    );
    SPDLOG_DEBUG(
      "[RDMAPassive]: listening id {}, protection domain {}",
      fmt::ptr(this->_listen_id), _pd->handle
    );
  }

  ibv_pd* RDMAPassive::pd() const
  {
    return this->_pd;
  }

  uint32_t RDMAPassive::listen_port() const
  {
    return this->_addr.port();
  }

  void RDMAPassive::set_nonblocking_poll()
  {
    int fd = this->_ec->fd;
    int flags = fcntl(fd, F_GETFL);
    int rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (rc < 0) {
      spdlog::error("Failed to change file descriptor of rdmacm event channel");
      return;
    }
  }

  bool RDMAPassive::nonblocking_poll_events(int timeout)
  {
    pollfd my_pollfd;
    my_pollfd.fd      = this->_ec->fd;
    my_pollfd.events  = POLLIN;
    my_pollfd.revents = 0;
    int rc = poll(&my_pollfd, 1, timeout);
    if (rc < 0) {
      spdlog::error("RDMA event poll failed");
      return false;
    }
    return rc > 0;
  }

  std::tuple<Connection*, ConnectionStatus> RDMAPassive::poll_events(/*bool share_cqs*/)
  {
    rdma_cm_event* event = nullptr;
		Connection* connection = nullptr;
	  RecvWorkCompletions* recv_wc = nullptr;
    ConnectionStatus status = ConnectionStatus::UNKNOWN;

    // Poll rdma cm events.
    if(rdma_get_cm_event(this->_ec, &event)) {
      spdlog::error("Event poll unsuccesful, reason {} {}", errno, strerror(errno));
      return std::make_tuple(nullptr, ConnectionStatus::UNKNOWN);
    }
    SPDLOG_DEBUG(
      "[RDMAPassive] received event: {}, status: {}",
      rdma_event_str(event->event), event->status
    );

    switch (event->event) { 
      case RDMA_CM_EVENT_CONNECT_REQUEST: {

        connection = new Connection{_recv_buf, true};

        if(event->param.conn.private_data_len != 0) {
          uint32_t data = *reinterpret_cast<const uint32_t*>(event->param.conn.private_data);
          connection->set_private_data(data);
          SPDLOG_DEBUG("[RDMAPassive] Connection request with private data {}", data);
        }
        else
          SPDLOG_DEBUG("[RDMAPassive] Connection request with no private data");

        // Store connection ptr for future events. 
        event->id->context = reinterpret_cast<void*>(connection);
        SPDLOG_DEBUG(
          "[RDMAPassive] Creating QP: id {} pd {} listener pd {}",
          fmt::ptr(connection->id()), fmt::ptr(_pd), fmt::ptr(this->_listen_id->pd)
        );

        uint8_t key = PrivateData{connection->private_data()}.key();
        auto it = _shared_recv_completions.find(key);

        if(it == _shared_recv_completions.end()) {

          it = _shared_recv_completions.find(0);
          if(it == _shared_recv_completions.end()) {

            SPDLOG_DEBUG("Allocate new queue, no sharing for key {}", key);
            // Make sure to allocate new completion queue when they're not reused.
            _cfg.attr.send_cq = _cfg.attr.recv_cq = nullptr;

          } else {

            SPDLOG_DEBUG("Allocate default shared queue for key {}", key);
            _cfg.attr.send_cq = std::get<2>((*it).second);;
            _cfg.attr.recv_cq = std::get<1>((*it).second);

          }

        } else {

          SPDLOG_DEBUG("Allocate existing shared queue for key {}", key);
          _cfg.attr.send_cq = std::get<2>((*it).second);
          _cfg.attr.recv_cq = std::get<1>((*it).second);

        }

        SPDLOG_DEBUG(
          "[RDMAPassive] Using CQ for creating a QP: send {} recv {}",
          fmt::ptr(_cfg.attr.send_cq),fmt::ptr(_cfg.attr.recv_cq)
        );

        // Alocate queue pair for the new connection
        impl::expect_zero(rdma_create_qp(event->id, _pd, &_cfg.attr));
        connection->initialize(event->id);
        SPDLOG_DEBUG(
          "[RDMAPassive] Created connection id {} qpnum {} qp {} send {} recv {}, completion channel {}",
          fmt::ptr(connection->id()), connection->qp()->qp_num, fmt::ptr(connection->qp()),
          fmt::ptr(connection->qp()->send_cq), fmt::ptr(connection->qp()->recv_cq),
          fmt::ptr(connection->completion_channel())
        );

        if(it != _shared_recv_completions.end()) {

          if(!std::get<1>((*it).second)) {
            std::get<1>((*it).second) = connection->qp()->recv_cq;
          }
        }

        status = ConnectionStatus::REQUESTED;
        _active_connections.insert(connection);
        break;
      } case RDMA_CM_EVENT_ESTABLISHED:
        SPDLOG_DEBUG(
          "[RDMAPassive] Connection is established for id {}, and connection {}",
          fmt::ptr(event->id), fmt::ptr(event->id->context)
        );
        connection = reinterpret_cast<Connection*>(event->id->context);
        status = ConnectionStatus::ESTABLISHED;
        break;
      case RDMA_CM_EVENT_DISCONNECTED:
        SPDLOG_DEBUG(
          "[RDMAPassive] Disconnect for id {}, and connection {}",
          fmt::ptr(event->id), fmt::ptr(event->id->context)
        );
        connection = reinterpret_cast<Connection*>(event->id->context);
        //connection->close();
        status = ConnectionStatus::DISCONNECTED;
        _active_connections.erase(connection);
        break;
      case RDMA_CM_EVENT_ADDR_ERROR:
      case RDMA_CM_EVENT_ROUTE_ERROR:
      case RDMA_CM_EVENT_CONNECT_ERROR:
      case RDMA_CM_EVENT_UNREACHABLE:
      case RDMA_CM_EVENT_REJECTED:
      case RDMA_CM_EVENT_ADDR_RESOLVED:
      case RDMA_CM_EVENT_ROUTE_RESOLVED:
        SPDLOG_DEBUG(
          "[RDMAPassive] Unexpected event: {}, status: {}\n",
          rdma_event_str(event->event), event->status
        );
        break;
      case RDMA_CM_EVENT_DEVICE_REMOVAL:
        spdlog::error("[RDMAPassive] Not implemented support for device removal!");
        throw std::runtime_error("Not implemented support for device removal");
        break;
      default:
        break;
    }
    rdma_ack_cm_event(event);

    return std::make_tuple(connection, status);
  }

  void RDMAPassive::register_shared_queue(uint16_t key, bool share_send_queue)
  {
    ibv_comp_channel* channel = ibv_create_comp_channel(_pd->context);
    ibv_cq* cq = ibv_create_cq(_pd->context, _cfg.attr.cap.max_recv_wr, nullptr, channel, 0);
    ibv_cq* send_cq = nullptr;
    if(share_send_queue) {
      send_cq = ibv_create_cq(_pd->context, _cfg.attr.cap.max_send_wr, nullptr, channel, 0);
    }

    _shared_recv_completions[key] = std::make_tuple(channel, cq, send_cq);
    SPDLOG_DEBUG("[RDMAPassive] Register CQ {} for key {}, channel {}", fmt::ptr(cq), key, fmt::ptr(cq->channel));
  }

  std::tuple<ibv_comp_channel*, ibv_cq*, ibv_cq*>* RDMAPassive::shared_queue(uint16_t key)
  {
    auto it = _shared_recv_completions.find(key);
    return it != _shared_recv_completions.end() ? &it->second : nullptr;
  }

  void RDMAPassive::reject(Connection* connection) {
    if(rdma_reject(connection->id(), nullptr, 0)) {
      spdlog::error("Conection rejection unsuccesful, reason {} {}", errno, strerror(errno));
    }
    SPDLOG_DEBUG("[RDMAPassive] Connection rejected at QP {}", fmt::ptr(connection->qp()));
  }

  void RDMAPassive::accept(Connection* connection) {
    if(rdma_accept(connection->id(), &_cfg.conn_param)) {
      spdlog::error("Conection accept unsuccesful, reason {} {}", errno, strerror(errno));
    }
    SPDLOG_DEBUG("[RDMAPassive] Connection accepted at QP {}", fmt::ptr(connection->qp()));
  }
}
