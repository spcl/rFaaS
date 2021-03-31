
#include <chrono>
#include <thread>

#include <sys/stat.h>
#include <sys/wait.h>
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
  
  ActiveExecutor::~ActiveExecutor() {}

  ProcessExecutor::ProcessExecutor(pid_t pid):
    _pid(pid)
  {}

  std::tuple<ProcessExecutor::Status,int> ProcessExecutor::check() const
  {
    int status;
    pid_t return_pid = waitpid(_pid, &status, WNOHANG);
    if(!return_pid) {
      return std::make_tuple(Status::RUNNING, 0);
    } else {
      if(WIFEXITED(status)) {
        return std::make_tuple(Status::FINISHED, WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        return std::make_tuple(Status::FINISHED_FAIL, WTERMSIG(status));
      }
    }
  }

  int ProcessExecutor::id() const
  {
    return static_cast<int>(_pid);
  }

  ProcessExecutor* ProcessExecutor::spawn(const rdmalib::AllocationRequest & request, const ExecutorSettings & exec)
  {
    // FIXME: doesn't work every well?
    //int rc = ibv_fork_init();
    int rc = 0;
    if(rc)
      exit(rc);

    int mypid = fork();
    if(mypid < 0) {
      spdlog::error("Fork failed! {}", mypid);
    }
    if(mypid == 0) {
      mypid = getpid();
      spdlog::info("Child fork begins work on PID {}", mypid);
      auto out_file = ("executor_" + std::to_string(mypid));
      // FIXME: number of input buffers
      std::string client_port = std::to_string(request.listen_port);
      std::string client_in_size = std::to_string(request.input_buf_size);
      std::string client_func_size = std::to_string(request.func_buf_size);
      std::string client_cores = std::to_string(request.cores);
      std::string client_timeout = std::to_string(request.hot_timeout);
      std::string executor_repetitions = std::to_string(exec.repetitions);
      std::string executor_warmups = std::to_string(exec.warmup_iters);
      std::string executor_recv_buf = std::to_string(exec.recv_buffer_size);
      std::string executor_max_inline = std::to_string(exec.max_inline_data);

      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);
      const char * argv[] = {
        "bin/executor",
        "-a", request.listen_address,
        "-p", client_port.c_str(),
        "--polling-mgr", "thread",
        "-r", executor_repetitions.c_str(),
        "-x", executor_recv_buf.c_str(),
        "-s", client_in_size.c_str(),
        "--fast", client_cores.c_str(),
        "--warmup-iters", executor_warmups.c_str(),
        "--max-inline-data", executor_max_inline.c_str(),
        "--func-size", client_func_size.c_str(),
        "--timeout", client_timeout.c_str(),
        nullptr
      };
      int ret = execve(argv[0], const_cast<char**>(&argv[0]), nullptr);
      spdlog::info("Child fork stopped work on PID {}", mypid);
      if(ret == -1) {
        spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
        close(fd);
        exit(1);
      }
      close(fd);
      exit(0);
    }
    return new ProcessExecutor{mypid};
  }

  Manager::Manager(std::string addr, int port, std::string server_file, const ExecutorSettings & settings):
    _clients_active(0),
    _state(addr, port, 32, true),
    _status(addr, port),
    _settings(settings)
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
              // FIXME: Docker
              client.executor.reset(
                ProcessExecutor::spawn(
                  client.allocation_requests.data()[id],
                  _settings
                )
              );
              spdlog::info(
                "Client {} at {}:{} has executor with {} ID",
                i, client_address, client_port, client.executor->id()
              );
            } else {
              spdlog::info("Client {} disconnects", i);
              client.disable();
              --_clients_active;
              break;
            }
          }
        }
        if(client.active()) {
          client.rcv_buffer.refill();
          if(client.executor) {
            auto status = client.executor->check();
            if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
              spdlog::info("Executor at client {} exited, status {}", i, std::get<1>(status));
              client.executor.reset(nullptr);
            }
          }
        }
      }
    }
  }

}
