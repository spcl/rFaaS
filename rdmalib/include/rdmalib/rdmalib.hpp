
#ifndef __RDMALIB_RDMALIB_HPP__
#define __RDMALIB_RDMALIB_HPP__

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <functional>

#include <rdma/rdma_cma.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>

namespace rdmalib {

  // Implemented as IPV4
  struct Address {
    rdma_addrinfo *addrinfo;
    rdma_addrinfo hints;
    uint16_t _port;

    Address(const std::string & ip, int port, bool passive);
    Address(const std::string & sip, const std::string & dip, int port);

    ~Address();
  };

  struct RDMAActive {
    ConnectionConfiguration _cfg;
    Connection _conn;
    Address _addr;
    rdma_event_channel * _ec;
    ibv_pd* _pd;

    RDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    ~RDMAActive();
    void allocate();
    bool connect();
    ibv_pd* pd() const;
    Connection & connection();
  };

  struct RDMAPassive {
    ConnectionConfiguration _cfg;
    Address _addr;
    rdma_event_channel * _ec;
    rdma_cm_id* _listen_id;
    ibv_pd* _pd;
    std::vector<Connection> _connections;

    RDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    ~RDMAPassive();
    void allocate();
    ibv_pd* pd() const;
    Connection* poll_events(std::function<void(Connection&)> before_accept = nullptr);

  };
}

#endif

