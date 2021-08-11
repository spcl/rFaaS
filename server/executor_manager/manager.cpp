
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

namespace rfaas::executor_manager {

  Manager::Manager(Settings & settings):
    //_clients_active(0),
    _q1(100), _q2(100),
    _active_connections(
        settings.resource_manager_address,
        settings.resource_manager_port,
        settings.device->default_receive_buffer_size
    ),
    _state(settings.device->ip_address, settings.rdma_device_port,
        settings.device->default_receive_buffer_size, true),
    _settings(settings),
    //_address(settings.device->ip_address),
    //_port(settings.rdma_device_port),
    _ids(0),
    // FIXME: randomly generated
    _secret(0x1234)
  {
    _active_connections.allocate();
  }

  void Manager::start()
  {
    spdlog::info(
      "Connecting to resource manager at {}:{} with secret {}.",
      _settings.resource_manager_address,
      _settings.resource_manager_port,
      _settings.resource_manager_secret
    );
    if(!_active_connections.connect(_settings.resource_manager_secret)) {
      spdlog::error("Connection to resource manager was not succesful!");
      return;
    }
    rdmalib::RecvBuffer _rcv_buffer{32};
    _rcv_buffer.connect(&_active_connections.connection());

    spdlog::info(
      "Begin listening at {}:{} and processing events!",
      _settings.device->ip_address,
      _settings.rdma_device_port
    );
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
      spdlog::debug("Listen for new rdmacm events");
      auto conn = _state.poll_events(
        false
      );
      if(conn == nullptr){
        spdlog::error("Failed connection creation");
        continue;
      }
      spdlog::debug("New connection request!");
      if(conn->_private_data) {
        if((conn->_private_data & 0xFFFF ) == this->_secret) {
          int client = conn->_private_data >> 16;
          SPDLOG_DEBUG("Executor for client {}", client);
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
        Client client{std::move(conn), _state.pd()};//, _accounting_data.data()[pos]};
        client._active = true;
        _state.accept(client.connection);

        _q2.enqueue(std::make_pair(pos, std::move(client)));    

        SPDLOG_DEBUG("send to another thread\n");
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
      {
        std::pair<int, std::unique_ptr<rdmalib::Connection> > *p1 = _q1.peek();
        if(p1){
          int client = p1->first;
          SPDLOG_DEBUG("Connected executor for client {}", client);
          int pos = _clients.find(client)->second.executor->connections_len++;
          _clients.find(client)->second.executor->connections[pos] = std::move(p1->second); 
        _q1.pop();
        }; 
        std::pair<int,Client>* p2 = _q2.peek();
        if(p2){
           int pos = p2->first;
          _clients.insert(std::make_pair(p2->first, std::move(p2->second)));
          SPDLOG_DEBUG("Connected new client id {}", pos);
          _q2.pop();
        };  
      }

      atomic_thread_fence(std::memory_order_acquire);
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
              spdlog::info(
                "Client {} requests executor with {} threads, it should connect to {}:{},"
                "it should have buffer of size {}, func buffer {}, and hot timeout {}",
                i, client.allocation_requests.data()[id].cores,
                client.allocation_requests.data()[id].listen_address,
                client.allocation_requests.data()[id].listen_port,
                client.allocation_requests.data()[id].input_buf_size,
                client.allocation_requests.data()[id].func_buf_size,
                client.allocation_requests.data()[id].hot_timeout
              );
              int secret = (i << 16) | (this->_secret & 0xFFFF);
              uint64_t addr = client.accounting.address(); //+ sizeof(Accounting)*i;
              // FIXME: Docker
              auto now = std::chrono::high_resolution_clock::now();
              client.executor.reset(
                ProcessExecutor::spawn(
                  client.allocation_requests.data()[id],
                  _settings.exec,
                  {
                    _settings.device->ip_address,
                    _settings.rdma_device_port,
                    secret, addr, client.accounting.rkey()
                  }
                )
              );
              auto end = std::chrono::high_resolution_clock::now();
              spdlog::info(
                "Client {} at {}:{} has executor with {} ID and {} cores, time {} us",
                i, client_address, client_port, client.executor->id(), cores,
                std::chrono::duration_cast<std::chrono::microseconds>(end-now).count()
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
              //client.disable(i, _accounting_data.data()[i]);
              client.disable(i);
              removals.push_back(it);
              break;
            }
          }
          //if(client.connection)
          //  client.connection->poll_wc(rdmalib::QueueType::SEND, false);
        }
        if(client.active()) {
          client.rcv_buffer.refill();
          if(client.executor) {
            //auto status = client.executor->check();
            //if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
            //  auto now = std::chrono::high_resolution_clock::now();
            //  client.allocation_time +=
            //    std::chrono::duration_cast<std::chrono::microseconds>(
            //      now - client.executor->_allocation_finished
            //    ).count();
            //  // FIXME: update global manager
            //  spdlog::info(
            //    "Executor at client {} exited, status {}, time allocated {} us, polling {} us, execution {} us",
            //    i, std::get<1>(status), client.allocation_time,
            //    client.accounting.data()[i].hot_polling_time,
            //    client.accounting.data()[i].execution_time
            //    //_accounting_data.data()[i].hot_polling_time,
            //    //_accounting_data.data()[i].execution_time
            //  );
            //  client.executor.reset(nullptr);
            //  spdlog::info("Finished cleanup");
            //}
          }
        }
      }
      if(removals.size()) {
        for(auto it : removals) {
          spdlog::info("Remove client id {}", it->first);
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
