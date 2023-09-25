#ifndef __RDMALIB_RDMALIB_HPP__
#define __RDMALIB_RDMALIB_HPP__

#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <cereal/archives/json.hpp>

// #ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <arpa/inet.h>
// #include <rdma/fi_ext_gni.h>
#ifdef USE_GNI_AUTH
extern "C" {
#include "rdmacred.h"
}
#else
#include <rdma/rdma_cma.h>
#endif

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/libraries.hpp>

namespace rdmalib {

  struct Configuration {

    static Configuration& get_instance();

    #ifdef USE_GNI_AUTH
    void configure_cookie(uint32_t cray_cookie);
    std::optional<uint64_t> cookie() const;
    std::optional<uint32_t> credential() const;
    bool is_configured() const;
    #endif

  private:

    Configuration();
    ~Configuration();

    std::once_flag _access_flag;
    #ifdef USE_GNI_AUTH
    drc_info_handle_t _credential_info;
    #endif
    uint64_t _cookie;
    uint32_t _credential;
    bool _is_configured;

    static Configuration& _get_instance();
    static Configuration _instance;

  };

  // Implemented as IPV4
  template <typename Derived, typename Library>
  struct Address {
    uint16_t _port;
    // TODO do usings here for addrinfo & hints
    #ifdef USE_GNI_AUTH
    uint64_t cookie;
    #endif

    Address(const std::string & ip, int port, bool passive);
    Address(const std::string & sip, const std::string & dip, int port);
    Address();

    ~Address();
  };

  struct LibfabricAddress : Address<LibfabricAddress, libfabric> {
    fi_info* addrinfo = nullptr;
    fi_info* hints = nullptr;
    fid_fabric* fabric = nullptr;
    std::string _ip;

    LibfabricAddress(const std::string & ip, int port, bool passive);
    LibfabricAddress(const std::string & sip, const std::string & dip, int port);
    LibfabricAddress() {}

    ~LibfabricAddress();
  };

  struct VerbsAddress : Address<VerbsAddress, ibverbs> {
    rdma_addrinfo *addrinfo;
    rdma_addrinfo hints;

    VerbsAddress(const std::string & ip, int port, bool passive);
    VerbsAddress(const std::string & sip, const std::string & dip, int port);
    VerbsAddress() {}

    ~VerbsAddress();
  };

  template <typename Derived, typename Library>
  struct RDMAActive {
    using pd_t = typename library_traits<Library>::pd_t;
    using Connection_t = typename rdmalib_traits<Library>::Connection;
    using Address_t = typename rdmalib_traits<Library>::Address;

    std::unique_ptr<Connection_t> _conn;
    Address_t _addr;

    RDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    RDMAActive() {}
    ~RDMAActive();
    void allocate();
    bool connect(uint32_t secret = 0);
    void disconnect();
    pd_t* pd() const
    {
      return static_cast<const Derived*>(this)->pd();
    }
    Connection_t & connection() { return *_conn; }
    bool is_connected() { return _conn.get(); }
  };

  struct LibfabricRDMAActive : RDMAActive<LibfabricRDMAActive, libfabric> {
    std::unique_ptr<LibfabricConnection> _conn;
    LibfabricAddress _addr;

    fid_eq* _ec = nullptr;
    fid_cq* _rcv_channel = nullptr;
    fid_cq* _trx_channel = nullptr;
    fid_cntr* _write_counter = nullptr;
    pd_t _pd;

    LibfabricRDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    LibfabricRDMAActive() {}
    ~LibfabricRDMAActive();
    void allocate();
    bool connect(uint32_t secret = 0);
    void disconnect();
    pd_t pd() const { return this->_pd; }
  };

  struct VerbsRDMAActive : RDMAActive<VerbsRDMAActive, ibverbs> {
    ConnectionConfiguration _cfg;
    std::unique_ptr<VerbsConnection> _conn;
    VerbsAddress _addr;
    rdma_event_channel * _ec;
    pd_t _pd;

    VerbsRDMAActive(const std::string & ip, int port, int recv_buf = 1, int max_inline_data = 0);
    VerbsRDMAActive() {}
    ~VerbsRDMAActive();
    void allocate();
    bool connect(uint32_t secret = 0);
    void disconnect();
    pd_t pd() const { return this->_pd; }
  };

  template <typename Derived, typename Library>
  struct RDMAPassive {
    using Connection_t = typename rdmalib_traits<Library>::Connection;
    using Address_t = typename rdmalib_traits<Library>::Address;
    using pd_t = typename library_traits<Library>::pd_t;

    Address_t _addr;
    // Set of connections that have been
    std::unordered_set<Connection_t*> _active_connections;

    RDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    RDMAPassive();
    ~RDMAPassive();
    void allocate();
    pd_t pd() const { return static_cast<const Derived*>(this)->pd(); }

    // Blocking poll for new rdmacm events.
    // Returns connection pointer and connection change status.
    // When connection is REQUESTED and ESTABLISHED, the pointer points to a valid connection.
    // When the status is DISCONNECTED, the pointer points to a closed connection.
    // User should deallocate the closed connection.
    // When the status is UNKNOWN, the pointer is null.
    std::tuple<Connection_t*, ConnectionStatus> poll_events(bool share_cqs = false);
    bool nonblocking_poll_events(int timeout = 100);
    void accept(Connection_t* connection);
    void set_nonblocking_poll();
  };

  struct LibfabricRDMAPassive : RDMAPassive<LibfabricRDMAPassive, libfabric> {
    LibfabricAddress _addr;
    fid_eq* _ec = nullptr;
    fid_domain* _pd = nullptr;
    fid_pep* _pep = nullptr;
    fid_cq* _rcv_channel;
    fid_cq* _trx_channel;
    fid_cntr* _write_counter = nullptr;
    // fi_gni_ops_domain* _ops;
    // Set of connections that have been
    std::unordered_set<LibfabricConnection*> _active_connections;

    LibfabricRDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    ~LibfabricRDMAPassive();
    void allocate();
    pd_t pd() const { return this->_pd; }

    // Blocking poll for new rdmacm events.
    // Returns connection pointer and connection change status.
    // When connection is REQUESTED and ESTABLISHED, the pointer points to a valid connection.
    // When the status is DISCONNECTED, the pointer points to a closed connection.
    // User should deallocate the closed connection.
    // When the status is UNKNOWN, the pointer is null.
    std::tuple<LibfabricConnection*, ConnectionStatus> poll_events(bool share_cqs = false);
    bool nonblocking_poll_events(int timeout = 100);
    void accept(LibfabricConnection* connection);
    void set_nonblocking_poll();
  };

  struct VerbsRDMAPassive : RDMAPassive<VerbsRDMAPassive, ibverbs> {
    ConnectionConfiguration _cfg;
    VerbsAddress _addr;
    rdma_event_channel * _ec;
    rdma_cm_id* _listen_id;
    ibv_pd* _pd;

    // Set of connections that have been
    std::unordered_set<VerbsConnection*> _active_connections;

    VerbsRDMAPassive(const std::string & ip, int port, int recv_buf = 1, bool initialize = true, int max_inline_data = 0);
    ~VerbsRDMAPassive();
    void allocate();
    pd_t pd() const { return this->_pd; }
    
    // Blocking poll for new rdmacm events.
    // Returns connection pointer and connection change status.
    // When connection is REQUESTED and ESTABLISHED, the pointer points to a valid connection.
    // When the status is DISCONNECTED, the pointer points to a closed connection.
    // User should deallocate the closed connection.
    // When the status is UNKNOWN, the pointer is null.
    std::tuple<VerbsConnection*, ConnectionStatus> poll_events(bool share_cqs = false);
    bool nonblocking_poll_events(int timeout = 100);
    void accept(VerbsConnection* connection);
    void set_nonblocking_poll();
  };

  namespace server {

    template <typename Library>
    ServerStatus<Library>::ServerStatus():
      _address(""),
      _port(0)
    {}

    template <typename Library>
    ServerStatus<Library>::ServerStatus(std::string address, int port):
      _address(address),
      _port(port)
    {}

    template <typename Library>
    ServerStatus<Library> ServerStatus<Library>::deserialize(std::istream & in)
    {
      ServerStatus status;
      cereal::JSONInputArchive archive_in(in);
      archive_in(status);
      return status;
    }

    template <typename Library>
    void ServerStatus<Library>::serialize(std::ostream & out) const
    {
      cereal::JSONOutputArchive archive_out(out);
      archive_out(*this);
    }
  }

}

#endif

