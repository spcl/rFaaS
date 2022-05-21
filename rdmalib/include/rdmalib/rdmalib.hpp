
#ifndef __RDMALIB_RDMALIB_HPP__
#define __RDMALIB_RDMALIB_HPP__

#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>
#include <functional>

#ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <arpa/inet.h>
// #include <rdma/fi_ext_gni.h>
#ifdef USE_GNI_AUTH
#include <mutex>
extern "C" {
#include "rdmacred.h"
}
#endif
#else
#include <rdma/rdma_cma.h>
#endif

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>

namespace rdmalib {

  // Implemented as IPV4
  struct Address {
    #ifdef USE_LIBFABRIC
    fi_info* addrinfo = nullptr;
    fi_info* hints = nullptr;
    fid_fabric* fabric = nullptr;
    #ifdef USE_GNI_AUTH
    std::once_flag access_flag;
    std::once_flag release_flag;
    void obtain_cookies();
    static drc_info_handle_t credential_info;
    static uint64_t cookie;
    #endif
    std::string _ip;
    #else
    rdma_addrinfo *addrinfo;
    rdma_addrinfo hints;
    #endif
    uint16_t _port;

    Address(const std::string & ip, int port, bool passive);
    Address(const std::string & sip, const std::string & dip, int port);
    Address();

    ~Address();
  };

  struct RDMAActive {
    #ifndef USE_LIBFABRIC
    ConnectionConfiguration _cfg;
    #endif
    std::unique_ptr<Connection> _conn;
    Address _addr;
    #ifdef USE_LIBFABRIC
    fid_eq* _ec = nullptr;
    fid_domain* _pd = nullptr;
    fid_cq* _rcv_channel = nullptr;
    fid_cq* _trx_channel = nullptr;
    #else
    rdma_event_channel * _ec;
    ibv_pd* _pd;
    #endif

    RDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    RDMAActive();
    ~RDMAActive();
    void allocate();
    bool connect(uint32_t secret = 0);
    void disconnect();
    #ifdef USE_LIBFABRIC
    fid_domain* pd() const;
    #else
    ibv_pd* pd() const;
    #endif
    Connection & connection();
    bool is_connected();
  };

  struct RDMAPassive {
    #ifndef USE_LIBFABRIC
    ConnectionConfiguration _cfg;
    #endif
    Address _addr;
    #ifdef USE_LIBFABRIC
    fid_eq* _ec = nullptr;
    fid_domain* _pd = nullptr;
    fid_pep* _pep = nullptr;
    fid_cq* _rcv_channel;
    fid_cq* _trx_channel;
    // fi_gni_ops_domain* _ops;
    #else
    rdma_event_channel * _ec;
    rdma_cm_id* _listen_id;
    ibv_pd* _pd;
    #endif
    // Set of connections that have been
    std::unordered_set<Connection*> _active_connections;

    RDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    ~RDMAPassive();
    void allocate();
    #ifdef USE_LIBFABRIC
    fid_domain* pd() const;
    #else
    ibv_pd* pd() const;
    #endif
    // Blocking poll for new rdmacm events.
    // Returns connection pointer and connection change status.
    // When connection is REQUESTED and ESTABLISHED, the pointer points to a valid connection.
    // When the status is DISCONNECTED, the pointer points to a closed connection.
    // User should deallocate the closed connection.
    // When the status is UNKNOWN, the pointer is null.
    std::tuple<Connection*, ConnectionStatus> poll_events(bool share_cqs = false);
    bool nonblocking_poll_events(int timeout = 100);
    void accept(Connection* connection);
    void set_nonblocking_poll();
  };
}

#endif

