
#include "rdmalib/connection.hpp"
#include <cassert>
#include <cstdint>
// inet_ntoa
#include <arpa/inet.h>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <netdb.h>

// poll on file descriptors
#include <poll.h>
#include <fcntl.h>
#include <rdma/fi_endpoint.h>

#ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_errno.h>
#ifdef USE_GNI_AUTH
extern "C" {
#include "rdmacred.h"
}
#endif
#endif
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/format.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/util.hpp>
#include <stdexcept>

namespace rdmalib {

  Configuration::Configuration():
    _is_configured(false)
  {}

  Configuration::~Configuration()
  {
    #ifdef USE_GNI_AUTH
    drc_release_local(&_credential_info);
    #endif
  }

  Configuration& Configuration::get_instance()
  {
    return _get_instance();
  }

  Configuration& Configuration::_get_instance()
  {
    static Configuration _instance;
    return _instance;
  }

  void Configuration::configure_cookie(uint32_t credential)
  {
    Configuration& inst = _get_instance();
    std::call_once(_access_flag, [&]() {

      impl::expect_zero(drc_access(credential, 0, &inst._credential_info));

      _credential = credential;
      // Obtain the cookie
      _cookie = (uint64_t)drc_get_first_cookie(inst._credential_info) << 32;

      _is_configured = true;

    });
  }

  std::optional<uint64_t> Configuration::cookie() const
  {
    return (_is_configured ? _cookie : std::optional<uint64_t>{});
  }

  std::optional<uint32_t> Configuration::credential() const
  {
    return (_is_configured ? _credential: std::optional<uint32_t>{});
  }

  bool Configuration::is_configured() const
  {
    return _is_configured;
  }

  Configuration Configuration::_instance;

  // FIXME: Add credential support
  Address::Address(const std::string & ip, int port, bool passive)
  {
    #ifdef USE_LIBFABRIC
    // Set the hints and addrinfo to clear structures
    hints = fi_allocinfo();
    addrinfo = fi_allocinfo();

    // Set the hints to have ability to conduct MSG, Atomic and RMA operations
    hints->caps |= FI_MSG | FI_RMA | FI_ATOMIC | FI_RMA_EVENT;
    // Set the hints to indicate that we will register the local buffers
    hints->domain_attr->mr_mode = FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY;
    hints->ep_attr->type = FI_EP_MSG;
    hints->fabric_attr->prov_name = strdup("GNI");
    hints->domain_attr->threading = FI_THREAD_SAFE;
    hints->domain_attr->resource_mgmt = FI_RM_ENABLED;
    hints->tx_attr->tclass = FI_TC_LOW_LATENCY;
    impl::expect_zero(fi_getinfo(FI_VERSION(1, 9), ip.c_str(), std::to_string(port).c_str(), passive ? FI_SOURCE : 0, hints, &addrinfo));
    fi_freeinfo(hints);
    impl::expect_zero(fi_fabric(addrinfo->fabric_attr, &fabric, nullptr));
    std::cerr << "True" << std::endl;
    #ifdef USE_GNI_AUTH

    // Obtain the cookies once per process
    auto c = Configuration::get_instance().cookie();
    // spdlog::error("GNI authentication cookie has not been configured!");
    impl::expect_true(c.has_value());
    cookie = c.value();

    // Set the hints to have the cookies
    addrinfo->domain_attr->auth_key = (uint8_t *)malloc(sizeof(cookie));
    memcpy(addrinfo->domain_attr->auth_key, &cookie, sizeof(cookie));
    addrinfo->domain_attr->auth_key_size = sizeof(cookie);
    addrinfo->ep_attr->auth_key = (uint8_t *)malloc(sizeof(cookie));
    memcpy(addrinfo->ep_attr->auth_key, &cookie, sizeof(cookie));
    addrinfo->ep_attr->auth_key_size = sizeof(cookie);
    spdlog::info("Saved Cray credentials cookie {}", cookie);
    #endif
    #else
    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if(passive)
      hints.ai_flags = RAI_PASSIVE;

    impl::expect_zero(rdma_getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &addrinfo));
    #endif
    this->_port = port;
    this->_ip = ip;
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

    #ifdef USE_LIBFABRIC
    // Set the hints and addrinfo to clear structures
    hints = fi_allocinfo();
    addrinfo = fi_allocinfo();

    // Set the hints to have ability to conduct MSG, Atomic and RMA operations
    hints->caps |= FI_MSG | FI_RMA | FI_ATOMIC | FI_RMA_EVENT;
    // Set the hints to indicate that we will register the local buffers
    hints->mode |= FI_LOCAL_MR; 
    free(hints->fabric_attr->prov_name);
    hints->fabric_attr->prov_name = strdup("verbs"); 
    
    // Set addresses and their format
    hints->addr_format = FI_SOCKADDR_IN;
    hints->src_addrlen = sizeof(local_in);
    hints->dest_addrlen = sizeof(server_in);
    hints->src_addr = &local_in;
    hints->dest_addr = &server_in;
    
    impl::expect_zero(fi_getinfo(FI_VERSION(1, 13), nullptr, nullptr, 0, hints, &addrinfo));
    fi_freeinfo(hints);
    fi_fabric(addrinfo->fabric_attr, &fabric, nullptr);
    #else
    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    hints.ai_src_len = sizeof(local_in);
    hints.ai_dst_len = sizeof(server_in);
    hints.ai_src_addr = (struct sockaddr *)(&local_in);
    hints.ai_dst_addr = (struct sockaddr *)(&server_in);

    impl::expect_zero(rdma_getaddrinfo(NULL, NULL, &hints, &addrinfo));
    #endif
    this->_port = port;
  }

  Address::Address() {}

  Address::~Address()
  {
    #ifdef USE_LIBFABRIC
    // TODO Check how to free those and if it's necessary at all.
    //      When closing the addringo we obtain a double free or corruption problem.
    //      It seems that the problem is coming from the the ep_attr.
    // if (fabric)
    //   impl::expect_zero(fi_close(&fabric->fid));
    // if (addrinfo)
    //   fi_freeinfo(addrinfo); 
    #else
    rdma_freeaddrinfo(addrinfo);
    #endif
  }

  RDMAActive::RDMAActive(const std::string & ip, int port, int recv_buf, int max_inline_data):
    _conn(nullptr),
    _addr(ip, port, false),
    _ec(nullptr),
    _pd(nullptr)
  {
    #ifdef USE_LIBFABRIC
    // Create a domain (need to do that now so that we can register memory for the domain)
    impl::expect_zero(fi_domain(_addr.fabric, _addr.addrinfo, &_pd, nullptr));

    // Create the counter
    fi_cntr_attr cntr_attr;
    cntr_attr.events = FI_CNTR_EVENTS_COMP;
    cntr_attr.wait_obj = FI_WAIT_UNSPEC;
    cntr_attr.wait_set = nullptr;
    cntr_attr.flags = 0;
    impl::expect_zero(fi_cntr_open(_pd, &cntr_attr, &_write_counter, nullptr));
    impl::expect_zero(fi_cntr_set(_write_counter, 0));

    // Create the completion queues
    fi_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.format = FI_CQ_FORMAT_DATA;
    cq_attr.wait_obj = FI_WAIT_NONE;
    cq_attr.wait_cond = FI_CQ_COND_NONE;
    cq_attr.wait_set = nullptr;
    cq_attr.size = _addr.addrinfo->rx_attr->size;
    impl::expect_zero(fi_cq_open(_pd, &cq_attr, &_trx_channel, nullptr));
    impl::expect_zero(fi_cq_open(_pd, &cq_attr, &_rcv_channel, nullptr));
    #else
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
    #endif
    SPDLOG_DEBUG("Create RDMAActive");
  }

  RDMAActive::RDMAActive() {}

  RDMAActive::~RDMAActive()
  {
    #ifdef USE_LIBFABRIC
    if (_pd)
      impl::expect_zero(fi_close(&_pd->fid));
    if (_ec)
      impl::expect_zero(fi_close(&_ec->fid));
    #else
    //ibv_dealloc_pd(this->_pd);
    #endif
    SPDLOG_DEBUG("Destroy RDMAActive");
  }

  void RDMAActive::allocate()
  {
    if(!_conn) {
      _conn = std::unique_ptr<Connection>(new Connection());
      #ifdef USE_LIBFABRIC
      // Enable the event queue
      fi_eq_attr eq_attr;
      memset(&eq_attr, 0, sizeof(eq_attr));
      eq_attr.size = 0;
      eq_attr.wait_obj = FI_WAIT_NONE;
      impl::expect_zero(fi_eq_open(_addr.fabric, &eq_attr, &_ec, NULL));
      // Create and enable the endpoint together with all the accompanying queues
      _conn->initialize(_addr.fabric, _pd, _addr.addrinfo, _ec, _write_counter, _rcv_channel, _trx_channel);
      #else
      rdma_cm_id* id;
      impl::expect_zero(rdma_create_ep(&id, _addr.addrinfo, nullptr, nullptr));
      impl::expect_zero(rdma_create_qp(id, _pd, &_cfg.attr));
      _conn->initialize(id);
      _pd = _conn->id()->pd;
      #endif

      //struct ibv_qp_attr attr;
      //struct ibv_qp_init_attr init_attr;
      //impl::expect_zero(ibv_query_qp(_conn->_qp, &attr, IBV_QP_DEST_QPN, &init_attr ));
      //SPDLOG_DEBUG("Created active connection id {} qp {} send {} recv {}", fmt::ptr(_conn->_id), fmt::ptr(_conn->_id->qp), fmt::ptr(_conn->_id->qp->send_cq), fmt::ptr(_conn->_id->qp->recv_cq));
      //SPDLOG_DEBUG("Local QPN {}, remote QPN {} ",_conn->_qp->qp_num, attr.dest_qp_num);
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
    #ifdef USE_LIBFABRIC
    uint32_t *param = nullptr;
    size_t paramlen = 0;
    if(secret) {
      param = &secret;
      paramlen = sizeof(secret);
      SPDLOG_DEBUG("Setting connection secret {} of length {}", *param, paramlen);
    }
    int ret = fi_connect(_conn->qp(), _addr.addrinfo->dest_addr, param, paramlen);
    if(ret) {
      spdlog::error("Connection unsuccessful, reason {} message {} errno {} message {}", ret, fi_strerror(ret), errno, strerror(errno));
      _conn.reset();
      _pd = nullptr;
      return false;
    }
    uint32_t event;
    fi_eq_entry entry;
    do {
      ret = fi_eq_read(_ec, &event, &entry, sizeof(entry), 0);
    } while (ret == -FI_EAGAIN);
    if (event == FI_CONNECTED)
      spdlog::debug(
        "[RDMAActive] Connection successful to {}:{}",
        _addr._ip, _addr._port
      );
    else {
      spdlog::error("Connection unsuccessful, reason {} message {} errno {} message {}", ret, fi_strerror(ret), errno, strerror(errno));
      _conn.reset();
      _pd = nullptr;
      return false;
    }
    #else
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
    #endif

    //struct ibv_qp_attr attr;
    //struct ibv_qp_init_attr init_attr;
    //impl::expect_zero(ibv_query_qp(_conn->_qp, &attr, IBV_QP_DEST_QPN, &init_attr ));
    //SPDLOG_DEBUG("Local QPN {}, remote QPN {} ",_conn->_qp->qp_num, attr.dest_qp_num);

    return true;
  }

  void RDMAActive::disconnect()
  {
    #ifdef USE_LIBFABRIC
    // TODO: Add the disconnectin id
    spdlog::debug("[RDMAActive] Disconnecting connection with id {}", fmt::ptr(&_conn->qp()->fid));
    _conn->close();
    _conn.reset();
    _pd = nullptr;
    #else
        spdlog::debug("[RDMAActive] Disonnecting connection with id {}", fmt::ptr(_conn->id()));
    impl::expect_zero(rdma_disconnect(_conn->id()));
    _conn.reset();
    _pd = nullptr;
    #endif
  }

  #ifdef USE_LIBFABRIC
  fid_domain* RDMAActive::pd() const
  {
    return this->_pd;
  }
  #else
  ibv_pd* RDMAActive::pd() const
  {
    return this->_pd;
  }
  #endif

  Connection & RDMAActive::connection()
  {
    return *this->_conn;
  }

  bool RDMAActive::is_connected()
  {
    return this->_conn.get();
  }

  RDMAPassive::RDMAPassive(const std::string & ip, int port, int recv_buf, bool initialize, int max_inline_data):
    _addr(ip, port, true),
    _ec(nullptr),
    #ifndef USE_LIBFABRIC
    _listen_id(nullptr),
    #endif
    _pd(nullptr)
  {
    #ifdef USE_LIBFABRIC
    impl::expect_zero(fi_domain(_addr.fabric, _addr.addrinfo, &_pd, nullptr));
    #else
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
    #endif

    if(initialize)
      this->allocate();
  }

  RDMAPassive::~RDMAPassive()
  {
    #ifdef USE_LIBFABRIC
    if (_pd)
      impl::expect_zero(fi_close(&_pd->fid));
    if (_pep)
      impl::expect_zero(fi_close(&_pep->fid));
    if (_ec)
      impl::expect_zero(fi_close(&_ec->fid));
    #else
    rdma_destroy_id(this->_listen_id);
    rdma_destroy_event_channel(this->_ec);
    #endif
  }

  void RDMAPassive::allocate()
  {
    #ifdef USE_LIBFABRIC
    // Start listening
    fi_eq_attr eq_attr;
    memset(&eq_attr, 0, sizeof(eq_attr));
    eq_attr.size = 0;
    eq_attr.wait_obj = FI_WAIT_UNSPEC;
    impl::expect_zero(fi_eq_open(_addr.fabric, &eq_attr, &_ec, NULL));
    impl::expect_zero(fi_passive_ep(_addr.fabric, _addr.addrinfo, &_pep, NULL));
    impl::expect_zero(fi_pep_bind(_pep, &(_ec->fid), 0));
    impl::expect_zero(fi_listen(_pep));

    // Create the counter
    fi_cntr_attr cntr_attr;
    cntr_attr.events = FI_CNTR_EVENTS_COMP;
    cntr_attr.wait_obj = FI_WAIT_UNSPEC;
    cntr_attr.wait_set = nullptr;
    cntr_attr.flags = 0;
    impl::expect_zero(fi_cntr_open(_pd, &cntr_attr, &_write_counter, nullptr));
    impl::expect_zero(fi_cntr_set(_write_counter, 0));

    // Create the completion queues
    fi_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.format = FI_CQ_FORMAT_DATA;
    cq_attr.wait_obj = FI_WAIT_NONE;
    cq_attr.wait_cond = FI_CQ_COND_NONE;
    cq_attr.wait_set = nullptr;
    cq_attr.size = _addr.addrinfo->rx_attr->size;
    impl::expect_zero(fi_cq_open(_pd, &cq_attr, &_trx_channel, nullptr));
    impl::expect_zero(fi_cq_open(_pd, &cq_attr, &_rcv_channel, nullptr));
    // _ops = (fi_gni_ops_domain *)malloc(sizeof(fi_gni_ops_domain));
    // fi_open_ops(&_pd->fid, "FI_GNI_DOMAIN_OPS_1", 0, (void **)*_ops, nullptr);
    // uint32_t val;
    // _ops->get_val(&_pd->fid, GNI_CONN_TABLE_MAX_SIZE, &val);
    // std::cout << "MAXIMUM VALUE: " << val << std::endl;
    #else
        // Start listening
    impl::expect_nonzero(this->_ec = rdma_create_event_channel());
    impl::expect_zero(rdma_create_id(this->_ec, &this->_listen_id, NULL, RDMA_PS_TCP));
    impl::expect_zero(rdma_bind_addr(this->_listen_id, this->_addr.addrinfo->ai_src_addr));
    impl::expect_zero(rdma_listen(this->_listen_id, 10));
    this->_addr._port = ntohs(rdma_get_src_port(this->_listen_id));
    this->_pd = _listen_id->pd;
    spdlog::info(
      "Listening on device {}, port {}",
      ibv_get_device_name(this->_listen_id->verbs->device), this->_addr._port
    );
    SPDLOG_DEBUG(
      "[RDMAPassive]: listening id {}, protection domain {}",
      fmt::ptr(this->_listen_id), _pd->handle
    );
    #endif
  }

  #ifdef USE_LIBFABRIC
  fid_domain* RDMAPassive::pd() const
  {
    return this->_pd;
  }
  #else
  ibv_pd* RDMAPassive::pd() const
  {
    return this->_pd;
  }
  #endif

  #ifndef USE_LIBFABRIC
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
  #endif

  bool RDMAPassive::nonblocking_poll_events(int timeout)
  {
    #ifdef USE_LIBFABRIC
    uint32_t event;
    fi_eq_entry entry;
    int ret = fi_eq_sread(_ec, &event, &entry, sizeof(entry), timeout, FI_PEEK);
    if (ret < 0 && ret != -FI_EAGAIN && ret != -FI_EAVAIL) 
      spdlog::error("RDMA event poll failed");
    return ret > 0 || ret == -FI_EAVAIL;
    #else
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
    #endif
  }

  std::tuple<Connection*, ConnectionStatus> RDMAPassive::poll_events(bool share_cqs)
  {
    #ifdef USE_LIBFABRIC
    uint32_t event;
    // Need those additional bytes in fi_eq_cm_entry so that we can transfer the secret
    int total_size = sizeof(fi_eq_cm_entry) + sizeof(uint32_t);
    fi_eq_cm_entry *entry = (fi_eq_cm_entry *)malloc(total_size);
  	Connection* connection = nullptr;
    ConnectionStatus status = ConnectionStatus::UNKNOWN;

    // Poll rdma cm events.
    int ret;
    do
      ret = fi_eq_read(_ec, &event, entry, total_size, 0);
    while (ret == -FI_EAGAIN);
    if(ret < 0) {
      spdlog::error("Event poll unsuccessful, return {} message {} errno {} message {}", ret, fi_strerror(ret), errno, strerror(errno));
      return std::make_tuple(nullptr, ConnectionStatus::UNKNOWN);
    }
    SPDLOG_DEBUG(
      "[RDMAPassive] received event: {} in text {}",
      event, fi_tostr(&event, FI_TYPE_EQ_EVENT)
    );

    switch (event) { 
      case FI_CONNREQ:
        connection = new Connection{true};

        SPDLOG_DEBUG("[RDMAPassive] Connection request with ret {}", ret);

        // Read the secret
        if(ret == total_size) {
          uint32_t data = *reinterpret_cast<const uint32_t*>(entry->data);
          connection->set_private_data(data);
          SPDLOG_DEBUG("[RDMAPassive] Connection request with private data {}", data);
        }
        else
          SPDLOG_DEBUG("[RDMAPassive] Connection request with no private data");

        // Check if we have a domain open for the connection already
        // if (!entry->info->domain_attr->domain)
        //   fi_domain(_addr.fabric, entry->info, &connection->_domain, NULL);

        // Enable the endpoint
        #ifdef USE_GNI_AUTH
        entry->info->domain_attr->auth_key = (uint8_t *)malloc(sizeof(_addr.cookie));
        memcpy(entry->info->domain_attr->auth_key, &_addr.cookie, sizeof(_addr.cookie));
        entry->info->domain_attr->auth_key_size = sizeof(_addr.cookie);
        entry->info->ep_attr->auth_key = (uint8_t *)malloc(sizeof(_addr.cookie));
        memcpy(entry->info->ep_attr->auth_key, &_addr.cookie, sizeof(_addr.cookie));
        entry->info->ep_attr->auth_key_size = sizeof(_addr.cookie);
        #endif
        connection->initialize(_addr.fabric, _pd, entry->info, _ec, _write_counter, _rcv_channel, _trx_channel);
        SPDLOG_DEBUG(
          "[RDMAPassive] Created connection fid {} qp {}",
          fmt::ptr(connection->id()), fmt::ptr(&connection->qp()->fid)
        );

        // Free the info
        fi_freeinfo(entry->info);

        status = ConnectionStatus::REQUESTED;
        _active_connections.insert(connection);
        break;
      case FI_CONNECTED:
        SPDLOG_DEBUG(
          "[RDMAPassive] Connection is established for id {}, and connection {}",
          fmt::ptr(entry->fid), fmt::ptr(entry->fid->context)
        );
        connection = reinterpret_cast<Connection*>(entry->fid->context);
        status = ConnectionStatus::ESTABLISHED;
        break;
      case FI_SHUTDOWN:
        SPDLOG_DEBUG(
          "[RDMAPassive] Disconnect for id {}, and connection {}",
          fmt::ptr(entry->fid), fmt::ptr(entry->fid->context)
        );
        connection = reinterpret_cast<Connection*>(entry->fid->context);
        //connection->close();
        status = ConnectionStatus::DISCONNECTED;
        _active_connections.erase(connection);
        break;
      default:
        spdlog::error("[RDMAPassive] Not any interesting event");
        break;
    }
    free(entry);
    #else
    rdma_cm_event* event = nullptr;
		Connection* connection = nullptr;
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
      case RDMA_CM_EVENT_CONNECT_REQUEST:
        connection = new Connection{true};
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

        // Make sure to allocate new completion queue when they're not reused.
        if(!share_cqs)
          _cfg.attr.send_cq = _cfg.attr.recv_cq = nullptr;
        SPDLOG_DEBUG(
          "[RDMAPassive] Using CQ for creating a QP: send {} recv {}",
          fmt::ptr(_cfg.attr.send_cq),fmt::ptr(_cfg.attr.recv_cq)
        );

        // Alocate queue pair for the new connection
        impl::expect_zero(rdma_create_qp(event->id, _pd, &_cfg.attr));
        connection->initialize(event->id);
        SPDLOG_DEBUG(
          "[RDMAPassive] Created connection id {} qpnum {} qp {} send {} recv {}",
          fmt::ptr(connection->id()), connection->qp()->qp_num, fmt::ptr(connection->qp()),
          fmt::ptr(connection->qp()->send_cq), fmt::ptr(connection->qp()->recv_cq)
        );

        status = ConnectionStatus::REQUESTED;
        _active_connections.insert(connection);
        break;
      case RDMA_CM_EVENT_ESTABLISHED:
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
    #endif

    return std::make_tuple(connection, status);
  }

  void RDMAPassive::accept(Connection* connection) {
    #ifdef USE_LIBFABRIC
    if(fi_accept(connection->qp(), nullptr, 0)) {
      spdlog::error("Conection accept unsuccessful, reason {} {}", errno, strerror(errno));
      connection = nullptr;
    }
    #else
    if(rdma_accept(connection->id(), &_cfg.conn_param)) {
      spdlog::error("Conection accept unsuccesful, reason {} {}", errno, strerror(errno));
      connection = nullptr;
    }
    #endif
    SPDLOG_DEBUG("[RDMAPassive] Connection accepted at QP {}", fmt::ptr(connection->qp()));
  }
}
