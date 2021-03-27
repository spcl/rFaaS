
#ifndef __SERVER_EXECUTOR_MANAGER__
#define __SERVER_EXECUTOR_MANAGER__

#include <cstdint>
#include <vector>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

namespace executor {

  struct Options {
    std::string address;
    int port;
    bool pin_threads;
    std::string server_file;
    bool verbose;
    bool use_docker;
  };
  Options opts(int, char**);


  struct ActiveExecutor
  {
    // pid; 

  };

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection& connection;
    rdmalib::Buffer<char> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    ActiveExecutor executor;

    Client(rdmalib::Connection& conn, ibv_pd* pd);
    void reload_queue();
  };

  struct Manager
  {
    std::vector<Client> _clients;
    rdmalib::RDMAPassive _state;
    rdmalib::server::ServerStatus _status;
    bool _use_docker;

    Manager(std::string addr, int port, bool use_docker, std::string server_file);

    void start();
    void listen();
    void poll_rdma();
  };

}

#endif

