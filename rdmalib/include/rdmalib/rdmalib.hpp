
#ifndef __RDMALIB_RDMALIB_HPP__
#define __RDMALIB_RDMALIB_HPP__

#include "rdmalib/queue.hpp"
#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>
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
    uint32_t _port;

    Address();
    Address(const std::string & ip, int port, bool passive);
    Address(const std::string & sip, const std::string & dip, int port);
    Address(Address && obj);
    Address& operator=(Address && obj);

    ~Address();

    void set_port(uint32_t port);
    uint32_t port() const;
  };

  struct RDMAActive {
    ConnectionConfiguration _cfg;
    std::unique_ptr<Connection> _conn;
    Address _addr;
    rdma_event_channel * _ec;
    ibv_pd* _pd;
    int _recv_buf;
    bool _is_connected;

    RDMAActive();
    RDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    RDMAActive & operator=(RDMAActive &&);
    ~RDMAActive();

    void allocate();
    bool connect(uint32_t secret = 0);
    void disconnect();
    ibv_pd* pd() const;
    Connection & connection();
    bool is_connected() const;
  };

  struct RDMAPassive {
    ConnectionConfiguration _cfg;
    Address _addr;
    rdma_event_channel * _ec;
    rdma_cm_id* _listen_id;
    ibv_pd* _pd;
    int _recv_buf;
    // Set of connections that have been
    std::unordered_set<Connection*> _active_connections;

    std::unordered_map<uint16_t, std::tuple<ibv_comp_channel*, ibv_cq*, ibv_cq*>> _shared_recv_completions;

    RDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    RDMAPassive(RDMAPassive && obj);
    ~RDMAPassive();

    RDMAPassive& operator=(RDMAPassive && obj);

    void allocate();
    ibv_pd* pd() const;
    uint32_t listen_port() const;

    // 0 is reserved value - it's a generic shared queue
    void register_shared_queue(uint16_t key, bool share_send_queue = false);
    std::tuple<ibv_comp_channel*, ibv_cq*, ibv_cq*>* shared_queue(uint16_t key);

    // Blocking poll for new rdmacm events.
    // Returns connection pointer and connection change status.
    // When connection is REQUESTED and ESTABLISHED, the pointer points to a valid connection.
    // When the status is DISCONNECTED, the pointer points to a closed connection.
    // User should deallocate the closed connection.
    // When the status is UNKNOWN, the pointer is null.
    std::tuple<Connection*, ConnectionStatus> poll_events();
    bool nonblocking_poll_events(int timeout = 100);
    void accept(Connection* connection);
    void reject(Connection* connection);
    void set_nonblocking_poll();
  };
}

#endif

