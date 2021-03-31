
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include <rdmalib/connection.hpp>
#include <rdmalib/allocation.hpp>

#include "manager.hpp"
#include "rdmalib/rdmalib.hpp"

namespace executor {

  Client::Client(rdmalib::Connection* conn, ibv_pd* pd):
    connection(conn),
    rcv_buffer(RECV_BUF_SIZE),
    allocation_requests(RECV_BUF_SIZE)
  {
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    // Initialize batch receive WCs
    conn->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
    rcv_buffer.connect(conn);
    SPDLOG_DEBUG("CREATED {}", connection != nullptr);
  }

  void Client::reinitialize(rdmalib::Connection* conn)
  {
    connection = conn;
    // Initialize batch receive WCs
    connection->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
    rcv_buffer.connect(conn);
  }

  void Client::reload_queue()
  {
    rcv_buffer.refill();
  }

  void Client::disable()
  {
    rdma_disconnect(connection->_id);
    // SEGFAULT?
    //ibv_dereg_mr(allocation_requests._mr);
    connection->close();
    connection = nullptr;
  }

  bool Client::active()
  {
    return connection;
  }

  Manager::Manager(std::string addr, int port, bool use_docker, std::string server_file):
    _clients_active(0),
    _state(addr, port, 32, true),
    _status(addr, port),
    _use_docker(use_docker)
  {
    _clients.reserve(this->MAX_CLIENTS_ACTIVE);
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
    //std::unique_ptr<rdmalib::RDMAPassive> passive;
    //passive.reset(new rdmalib::RDMAPassive("192.168.0.12", port++, 32, true));
    while(true) {
      // Connection initialization:
      // (1) Initialize receive WCs with the allocation request buffer
      //auto conn = passive->poll_events(
      //  [this](rdmalib::Connection & conn) {
      //    int i = 0;
      //    SPDLOG_DEBUG("NEW CONNECTION {}, _clients_active");
      //    _clients.emplace_back(&conn, _state.pd());
      //    _clients_active++;
      //  }
      //);
      //std::this_thread::sleep_for(std::chrono::seconds(1));
      //passive.reset();
      //std::this_thread::sleep_for(std::chrono::seconds(1));
      auto conn = _state.poll_events(
        [this](rdmalib::Connection & conn) {
          SPDLOG_DEBUG("NEW CONNECTION");

          _clients.emplace_back(&conn, _state.pd());
        }
      );
      atomic_thread_fence(std::memory_order_release);
      _clients_active++;
    }
  }

  void Manager::poll_rdma()
  {
    // FIXME: sleep when there are no clients
    bool active_clients = true;
    while(active_clients) {
      // FIXME: not safe? memory fance
      atomic_thread_fence(std::memory_order_acquire);
      for(int i = 0; i < _clients.size(); ++i) {
        Client & client = _clients[i];
        if(!client.active())
          continue;
        auto wcs = client.connection->poll_wc(rdmalib::QueueType::RECV, false);
        if(std::get<1>(wcs)) {
          spdlog::error("RECEIVED! {} wcs {} clients {} size {}", i,std::get<1>(wcs), _clients_active, _clients.size());
          for(int i = 0; i < std::get<1>(wcs); ++i) {
            auto wc = std::get<0>(wcs)[i];
            spdlog::error("wc {} {}", wc.wr_id, ibv_wc_status_str(wc.status));
            if(wc.status != 0)
              continue;
            uint64_t id = wc.wr_id;
            int16_t cores = client.allocation_requests.data()[id].cores;
            if(cores > 0) {
              SPDLOG_DEBUG("Client wants {} cores", cores);
            } else {
              client.disable();
              --_clients_active;
              break;
            }
          }
        }
        if(client.active())
          client.rcv_buffer.refill();
      }
    }
  }

}
