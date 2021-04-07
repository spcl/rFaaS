
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

  Client::Client(std::unique_ptr<rdmalib::Connection> conn, ibv_pd* pd, Accounting & _acc):
    connection(std::move(conn)),
    allocation_requests(RECV_BUF_SIZE),
    rcv_buffer(RECV_BUF_SIZE),
    accounting(_acc),
    allocation_time(0),
    _active(false)
  {
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    // Initialize batch receive WCs
    connection->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
    rcv_buffer.connect(connection.get());
  }

  //void Client::reinitialize(rdmalib::Connection* conn)
  //{
  //  connection = conn;
  //  // Initialize batch receive WCs
  //  connection->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
  //  rcv_buffer.connect(conn);
  //}

  void Client::reload_queue()
  {
    rcv_buffer.refill();
  }

  void Client::disable(int id, Accounting & acc)
  {
    rdma_disconnect(connection->_id);
    //kill(executor->id(), SIGKILL);
    // First, we check if the child is still alive
    int status;
    waitpid(executor->id(), &status, WUNTRACED);
    spdlog::info("Should be dead?");
    //auto status = executor->check();
    //spdlog::info("executor {} {}", executor->id(), std::get<0>(status) == ActiveExecutor::Status::RUNNING);
    //if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
    //  if(std::get<0>(status) != ActiveExecutor::Status::FINISHED)
    //    spdlog::info(
    //      "Executor at client {} exited, status {}",
    //      id, std::get<1>(status)
    //    );
    //  else
    //    spdlog::error(
    //      "Executor at client {} failed, status {}",
    //      id, std::get<1>(status)
    //    );
    //} else {
    //  // FIXME: kill executor

    //}
    executor.reset();
    spdlog::info(
      "Client {} exited, time allocated {} us, polling {} us, execution {} us",
      id, allocation_time,
      acc.hot_polling_time,
      acc.execution_time
    );
    acc.hot_polling_time = acc.execution_time = 0;
    // SEGFAULT?
    //ibv_dereg_mr(allocation_requests._mr);
    connection->close();
    connection = nullptr;
    _active=false;
  }

  bool Client::active()
  {
    // Compiler complains for some reason
    //return connection.operator bool();
    return _active;
  }
  
  ActiveExecutor::~ActiveExecutor()
  {
    delete[] connections; 
  }

  ProcessExecutor::ProcessExecutor(int cores, ProcessExecutor::time_t alloc_begin, pid_t pid):
    ActiveExecutor(cores),
    _pid(pid)
  {
    _allocation_begin = alloc_begin;
    // FIXME: remove after connection
    _allocation_finished = _allocation_begin;
  }

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
      } else {
        // Unknown problem
        return std::make_tuple(Status::FINISHED_FAIL, -1);
      }
    }
  }

  int ProcessExecutor::id() const
  {
    return static_cast<int>(_pid);
  }

  ProcessExecutor* ProcessExecutor::spawn(
    const rdmalib::AllocationRequest & request,
    const ExecutorSettings & exec,
    const executor::ManagerConnection & conn
  )
  {
    // FIXME: doesn't work every well?
    //int rc = ibv_fork_init();
    int rc = 0;
    if(rc)
      exit(rc);

    auto begin = std::chrono::high_resolution_clock::now();
    int mypid = fork();
    if(mypid < 0) {
      spdlog::error("Fork failed! {}", mypid);
    }
    if(mypid == 0) {
      mypid = getpid();
      spdlog::info("Child fork begins work on PID {}", mypid);
      auto out_file = ("executor_" + std::to_string(mypid));
      std::string client_port = std::to_string(request.listen_port);
      std::string client_in_size = std::to_string(request.input_buf_size);
      std::string client_func_size = std::to_string(request.func_buf_size);
      std::string client_cores = std::to_string(request.cores);
      std::string client_timeout = std::to_string(request.hot_timeout);
      std::string executor_repetitions = std::to_string(exec.repetitions);
      std::string executor_warmups = std::to_string(exec.warmup_iters);
      std::string executor_recv_buf = std::to_string(exec.recv_buffer_size);
      std::string executor_max_inline = std::to_string(exec.max_inline_data);

      std::string mgr_port = std::to_string(conn.port);
      std::string mgr_secret = std::to_string(conn.secret);
      std::string mgr_buf_addr = std::to_string(conn.r_addr);
      std::string mgr_buf_rkey = std::to_string(conn.r_key);

      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);
      const char * argv[] = {
        "executor",
        "-a", request.listen_address,
        "-p", client_port.c_str(),
        "--polling-mgr", "thread",
        "-r", executor_repetitions.c_str(),
        "-x", executor_recv_buf.c_str(),
        "-s", client_in_size.c_str(),
        "--pin-threads", "true",
        "--fast", client_cores.c_str(),
        "--warmup-iters", executor_warmups.c_str(),
        "--max-inline-data", executor_max_inline.c_str(),
        "--func-size", client_func_size.c_str(),
        "--timeout", client_timeout.c_str(),
        "--mgr-address", conn.addr.c_str(),
        "--mgr-port", mgr_port.c_str(),
        "--mgr-secret", mgr_secret.c_str(),
        "--mgr-buf-addr", mgr_buf_addr.c_str(),
        "--mgr-buf-rkey", mgr_buf_rkey.c_str(),
        nullptr
      };
      int ret = execvp(argv[0], const_cast<char**>(&argv[0]));
      spdlog::info("Child fork stopped work on PID {}", mypid);
      if(ret == -1) {
        spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
        close(fd);
        exit(1);
      }
      close(fd);
      exit(0);
    }
    return new ProcessExecutor{request.cores, begin, mypid};
  }

  Manager::Manager(std::string addr, int port, std::string server_file, const ExecutorSettings & settings):
    //_clients_active(0),
    _q1(100), _q2(100),
    _state(addr, port, 32, true),
    _status(addr, port),
    _settings(settings),
    _accounting_data(MAX_CLIENTS_ACTIVE),
    _address(addr),
    _port(port),
    // FIXME: randomly generated
    _secret(0x1234),
    _ids(0)
  {
    //_clients.reserve(this->MAX_CLIENTS_ACTIVE);
    std::ofstream out(server_file);
    _status.serialize(out);

    memset(_accounting_data.data(), 0, _accounting_data.data_size());
    _accounting_data.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
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
        false
      );
      if(conn == nullptr){
        printf("failed event\n");
        continue;
      }
      if(conn->_private_data) {
        if((conn->_private_data & 0xFFFF ) == this->_secret) {
          int client = conn->_private_data >> 16;
         // spdlog::info("Connected executor for client {}", client);
          _state.accept(conn);
          // FIXME: check it exists

          _q1.enqueue(std::make_pair( client, std::move(conn) ));    

          //int pos = _clients.find(client)->second.executor->connections_len++;
          //_clients.find(client)->second.executor->connections[pos] = std::move(conn); 


          //_clients[client].executor->connections[pos] = std::move(conn);

          //if(_clients[client].executor->connections_len == _clients[client].executor->cores) {
            // FIXME: alloc time
          //}
        } else {
          spdlog::error("New connection's private data that we can't understand: {}", conn->_private_data);
        }
      }
      // FIXME: users sending their ID 
      else {
        //_clients.emplace_back(std::move(conn), _state.pd(), _accounting_data.data()[_clients_active]);
        //_state.accept(_clients.back().connection);
        //spdlog::info("Connected new client id {}", _clients_active);
        ////_clients.back().connection->poll_wc(rdmalib::QueueType::RECV, true);
        //_clients.back()._active = true;
        //spdlog::info("Connected new client poll id {}", _clients_active);
        //atomic_thread_fence(std::memory_order_release);
        //_clients_active++;
        //int pos = _clients.size();
        int pos = _ids++;
        Client client{std::move(conn), _state.pd(), _accounting_data.data()[pos]};
        client._active = true;
        _state.accept(client.connection);
        _q2.enqueue(std::make_pair(pos, std::move(client)));    

       // {
    //      std::lock_guard<std::mutex> lock{clients};
    //      _clients.insert(std::make_pair(pos, std::move(client)));
    //    }

        atomic_thread_fence(std::memory_order_release);
       // spdlog::info("Connected new client id {}", pos);
        //spdlog::info("Connected new client poll id {}", _clients_active);
      }
    }
  }

  void Manager::poll_rdma()
  {
    // FIXME: sleep when there are no clients
    bool active_clients = true;
    while(active_clients) {

      atomic_thread_fence(std::memory_order_acquire);
      {
        std::pair<int, std::unique_ptr<rdmalib::Connection> > *p1 = _q1.peek();
        if(p1){
          int client = p1->first;
          spdlog::info("Connected executor for client {}", client);
          int pos = _clients.find(client)->second.executor->connections_len++;
          _clients.find(client)->second.executor->connections[pos] = std::move(p1->second); 
        }; 
        _q1.pop();
        std::pair<int,Client>* p2 = _q2.peek();
        if(p2){
           int pos = p2->first;
          _clients.insert(std::make_pair(p2->first, std::move(p2->second)));
          spdlog::info("Connected new client id {}", pos);
        };  
        _q2.pop();
      }


      std::vector<std::map<int, Client>::iterator> removals;
      for(auto it = _clients.begin(); it != _clients.end(); ++it) {
        Client & client = it->second;
        int i = it->first;
        auto wcs = client.rcv_buffer.poll(false);
        if(std::get<1>(wcs)) {
          SPDLOG_DEBUG(
            "Received at {}, work completions {}",
            i, std::get<1>(wcs)
          );
          for(int j = 0; j < std::get<1>(wcs); ++j) {
            auto wc = std::get<0>(wcs)[j];
            if(wc.status != 0)
              continue;
            uint64_t id = wc.wr_id;
            int16_t cores = client.allocation_requests.data()[id].cores;
            char * client_address = client.allocation_requests.data()[id].listen_address;
            int client_port = client.allocation_requests.data()[id].listen_port;

            if(cores > 0) {
              int secret = (i << 16) | (this->_secret & 0xFFFF);
              uint64_t addr = _accounting_data.address() + sizeof(Accounting)*i;
              // FIXME: Docker
              client.executor.reset(
                ProcessExecutor::spawn(
                  client.allocation_requests.data()[id],
                  _settings,
                  {this->_address, this->_port, secret, addr, _accounting_data.rkey()}
                )
              );
              spdlog::info(
                "Client {} at {}:{} has executor with {} ID and {} cores",
                i, client_address, client_port, client.executor->id(), cores
              );
            } else {
              spdlog::info("Client {} disconnects", i);
              if(client.executor) {
                auto now = std::chrono::high_resolution_clock::now();
                client.allocation_time +=
                  std::chrono::duration_cast<std::chrono::microseconds>(
                    now - client.executor->_allocation_finished
                  ).count();
              }
              client.disable(i, _accounting_data.data()[i]);
              removals.push_back(it);
              break;
            }
          }
        }
        if(client.active()) {
          client.rcv_buffer.refill();
          if(client.executor) {
            auto status = client.executor->check();
            if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
              auto now = std::chrono::high_resolution_clock::now();
              client.allocation_time +=
                std::chrono::duration_cast<std::chrono::microseconds>(
                  now - client.executor->_allocation_finished
                ).count();
              // FIXME: update global manager
              spdlog::info(
                "Executor at client {} exited, status {}, time allocated {} us, polling {} us, execution {} us",
                i, std::get<1>(status), client.allocation_time,
                _accounting_data.data()[i].hot_polling_time,
                _accounting_data.data()[i].execution_time
              );
              client.executor.reset(nullptr);
            }
          }
        }
      }
      if(removals.size()) {
    //    std::lock_guard<std::mutex> lock{clients};
        for(auto it : removals) {
          SPDLOG_DEBUG("Remove client id {}", it->first);
          _clients.erase(it);
        }
      }
    }
  }

  //void Manager::poll_rdma()
  //{
  //  // FIXME: sleep when there are no clients
  //  bool active_clients = true;
  //  while(active_clients) {
  //    // FIXME: not safe? memory fance
  //    atomic_thread_fence(std::memory_order_acquire);
  //    for(int i = 0; i < _clients.size(); ++i) {
  //      Client & client = _clients[i];
  //      if(!client.active())
  //        continue;
  //      //auto wcs = client.connection->poll_wc(rdmalib::QueueType::RECV, false);
  //      auto wcs = client.rcv_buffer.poll(false);
  //      if(std::get<1>(wcs)) {
  //        SPDLOG_DEBUG(
  //          "Received at {}, work completions {}, clients active {}, clients datastructure size {}",
  //          i, std::get<1>(wcs), _clients_active, _clients.size()
  //        );
  //        for(int j = 0; j < std::get<1>(wcs); ++j) {
  //          auto wc = std::get<0>(wcs)[j];
  //          if(wc.status != 0)
  //            continue;
  //          uint64_t id = wc.wr_id;
  //          int16_t cores = client.allocation_requests.data()[id].cores;
  //          char * client_address = client.allocation_requests.data()[id].listen_address;
  //          int client_port = client.allocation_requests.data()[id].listen_port;

  //          if(cores > 0) {
  //            int secret = (i << 16) | (this->_secret & 0xFFFF);
  //            uint64_t addr = _accounting_data.address() + sizeof(Accounting)*i;
  //            // FIXME: Docker
  //            client.executor.reset(
  //              ProcessExecutor::spawn(
  //                client.allocation_requests.data()[id],
  //                _settings,
  //                {this->_address, this->_port, secret, addr, _accounting_data.rkey()}
  //              )
  //            );
  //            spdlog::info(
  //              "Client {} at {}:{} has executor with {} ID and {} cores",
  //              i, client_address, client_port, client.executor->id(), cores
  //            );
  //          } else {
  //            spdlog::info("Client {} disconnects", i);
  //            if(client.executor) {
  //              auto now = std::chrono::high_resolution_clock::now();
  //              client.allocation_time +=
  //                std::chrono::duration_cast<std::chrono::microseconds>(
  //                  now - client.executor->_allocation_finished
  //                ).count();
  //            }
  //            client.disable(i, _accounting_data.data()[i]);
  //            --_clients_active;
  //            break;
  //          }
  //        }
  //      }
  //      if(client.active()) {
  //        client.rcv_buffer.refill();
  //        if(client.executor) {
  //          auto status = client.executor->check();
  //          if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
  //            auto now = std::chrono::high_resolution_clock::now();
  //            client.allocation_time +=
  //              std::chrono::duration_cast<std::chrono::microseconds>(
  //                now - client.executor->_allocation_finished
  //              ).count();
  //            // FIXME: update global manager
  //            spdlog::info(
  //              "Executor at client {} exited, status {}, time allocated {} us, polling {} us, execution {} us",
  //              i, std::get<1>(status), client.allocation_time,
  //              _accounting_data.data()[i].hot_polling_time,
  //              _accounting_data.data()[i].execution_time
  //            );
  //            client.executor.reset(nullptr);
  //          }
  //        }
  //      }
  //    }
  //  }
  //}

}
