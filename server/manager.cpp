
#include <thread>

#include <spdlog/spdlog.h>

#include <rdmalib/connection.hpp>
#include <rdmalib/allocation.hpp>

#include "manager.hpp"

namespace executor {

  Client::Client(rdmalib::Connection& conn, ibv_pd* pd):
    connection(conn),
    rcv_buffer(RECV_BUF_SIZE),
    allocation_requests(sizeof(rdmalib::AllocationRequest) * RECV_BUF_SIZE)
  {
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    // Initialize batch receive WCs
    conn.initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
    rcv_buffer.connect(&conn);
  }

  void Client::reload_queue()
  {
    rcv_buffer.refill();
  }

  Manager::Manager(std::string addr, int port, bool use_docker, std::string server_file):
    _state(addr, port, 32, true),
    _status(addr, port),
    _use_docker(use_docker)
  {
    std::ofstream out(server_file);
    _status.serialize(out);
  }

  void Manager::start()
  {
    spdlog::info("Begin listening and processing events!");
    std::thread listener(&Manager::listen, this);
    std::thread rdma_poller(&Manager::poll_rdma, this);

    listener.join();
    rdma_poller.join();
  }

  void Manager::listen()
  {
    while(true) {
      // Connection initialization:
      // (1) Initialize receive WCs with the allocation request buffer
      auto conn = _state.poll_events(
        [this](rdmalib::Connection & conn) {
          _clients.emplace_back(conn, _state.pd());
        }
      );
    }
  }

  void Manager::poll_rdma()
  {
    // FIXME: sleep when there are no clients
    bool active_clients = true;
    while(active_clients) {
      for(Client & client : _clients) {

      }
    }
  }

}
