
#include <chrono>
#include <thread>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

  int fork_process(std::string addr, int port, int cores)
  {
    static int counter = 1;
    //int rc = ibv_fork_init();
    int rc = 0;
    if(rc)
      exit(rc);

    int mypid = fork();
    if(mypid < 0)
      spdlog::error("Fork failed! {}", mypid);
    if(mypid == 0) {
      spdlog::info("Child fork begins work");
      auto out_file = ("exec" + std::to_string(counter));
      std::string client_port = std::to_string(port);
      std::string client_cores = std::to_string(cores);
      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      spdlog::info("Child fork opens fd {}", fd);
      dup2(fd, 1);
      dup2(fd, 2);
      const char * argv[] = {
        "bin/executor",
        "-a", addr.c_str(),
        "-p", client_port.c_str(),
        "-f", "server.json",
        "--polling-mgr", "thread",
        "-r", "10000",
        "-x", "32",
        "-s", "32",
        "--fast", client_cores.c_str(),
        "--warmup-iters", "100",
        "--max-inline-data", "128",
        "--func-size", "18496",
        "--timeout", "-1",
        nullptr
      };
      int ret = execve(argv[0], const_cast<char**>(&argv[0]), nullptr);
      spdlog::info("Child fork stopped work");
      if(ret == -1) {
        spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
        close(fd);
        exit(1);
      }
      close(fd);
      exit(0);
    }
    spdlog::error("Fork prepare {}", mypid);
    counter++;
    return mypid;
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
      auto conn = _state.poll_events(
        [this](rdmalib::Connection & conn) {
          _clients.emplace_back(&conn, _state.pd());
        },
        false
      );
      spdlog::info("Connected new client id {}", _clients_active);
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
          for(int j = 0; j < std::get<1>(wcs); ++j) {
            auto wc = std::get<0>(wcs)[j];
            spdlog::error("wc {} {}", wc.wr_id, ibv_wc_status_str(wc.status));
            if(wc.status != 0)
              continue;
            uint64_t id = wc.wr_id;
            int16_t cores = client.allocation_requests.data()[id].cores;
            char * client_address = client.allocation_requests.data()[id].listen_address;
            int client_port = client.allocation_requests.data()[id].listen_port;
            if(cores > 0) {
              spdlog::info("Client {} at {}:{} wants {} cores", i, client_address, client_port, cores);
              int pid = fork_process(client_address, client_port, cores);
              spdlog::info("Client {} at {}:{} has executor with {} pid", i, client_address, client_port, pid);
            } else {
              spdlog::info("Client {} disconnects", i);
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
