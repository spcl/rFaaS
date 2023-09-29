
#include <chrono>
#include <mutex>
#include <thread>
#include <variant>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <infiniband/verbs.h>
#include <spdlog/spdlog.h>

#include <rdmalib/connection.hpp>
#include <rdmalib/poller.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>

#include "manager.hpp"

namespace rfaas::executor_manager {

  void Leases::insert(Lease && obj)
  {
    _leases.try_emplace(obj.id, obj) ;
  }

  void Leases::insert_threadsafe(Lease && obj)
  {
    std::unique_lock lock{_mutex};
    _leases.try_emplace(obj.id, obj) ;
  }

  std::optional<Lease> Leases::get(int id)
  {
    auto it = _leases.find(id);
    if(it != _leases.end()) {
      Lease lease = (*it).second;
      _leases.erase(it);
      return lease;
    } else {
      return std::nullopt;
    }
  }

  std::optional<Lease> Leases::get_threadsafe(int id)
  {
    std::unique_lock lock{_mutex};

    auto it = _leases.find(id);
    if(it != _leases.end()) {
      Lease lease = (*it).second;
      _leases.erase(it);
      return lease;
    } else {
      return std::nullopt;
    }
  }

  constexpr int Manager::POLLING_TIMEOUT_MS;

  Manager::Manager(Settings & settings, bool skip_rm):
    _client_queue(100),
    _ids(0),
    _res_mgr_connection(nullptr),
    _state(settings.device->ip_address, settings.rdma_device_port,
        settings.device->default_receive_buffer_size, true),
    _client_responses(1),
    _settings(settings),
    _skip_rm(skip_rm),
    _shutdown(false)
  {
    if(!_skip_rm) {
      _res_mgr_connection = std::make_unique<ResourceManagerConnection>(
        settings.resource_manager_address,
        settings.resource_manager_port,
        settings.device->default_receive_buffer_size
      );
    }
  }

  void Manager::shutdown()
  {
    _shutdown.store(true);
  }

  void Manager::start()
  {
    if(!_skip_rm) {
      spdlog::info(
        "Connecting to resource manager at {}:{} with secret {}.",
        _settings.resource_manager_address,
        _settings.resource_manager_port,
        _settings.resource_manager_secret
      );

      rdmalib::PrivateData data;
      data.secret(_settings.resource_manager_secret);
      data.key(1);
      rdmalib::impl::expect_true(_res_mgr_connection->connect(_settings.node_name, data.data()));
    }

    _state.register_shared_queue(0);
    _client_responses.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);

    spdlog::info(
      "Begin listening at {}:{} and processing events!",
      _settings.device->ip_address,
      _settings.rdma_device_port
    );
    std::thread listener(&Manager::listen, this);
    std::thread rdma_poller(&Manager::poll_rdma, this);
    std::thread res_mgr_poller(&Manager::poll_res_mgr, this);

    res_mgr_poller.join();
    listener.join();
    rdma_poller.join(); 
  }

  void Manager::listen()
  {
    // FIXME: sleep when there are no clients
    while(!_shutdown.load()) {

      bool result = _state.nonblocking_poll_events(POLLING_TIMEOUT_MS);
      if(!result)
        continue;
      spdlog::debug("[Manager-listen] Polled new rdmacm event");

      auto [conn, conn_status] = _state.poll_events();
      spdlog::debug(
        "[Manager-listen] New rdmacm connection event - connection {}, status {}",
        fmt::ptr(conn), conn_status
      );
      if(conn == nullptr){
        spdlog::error("Failed connection creation");
        continue;
      }
      if(conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {
        // FIXME: handle disconnect
        spdlog::debug("[Manager-listen] Disconnection on connection {}", fmt::ptr(conn));
        _client_queue.emplace(Operation::DISCONNECT, msg_t{conn});
        continue;
      }
      // When client connects, we need to fill the receive queue with work requests before
      // accepting connection. Otherwise, we could accept before we're ready to receive data.
      else if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
        spdlog::debug("[Manager-listen] Requested new connection {}, private {}", fmt::ptr(conn), conn->private_data());
        rdmalib::PrivateData<0, 0, 32> private_data{conn->private_data()};

        if (private_data.secret() > 0) {

          _client_queue.emplace(Operation::CONNECT, msg_t{conn});

        } else {
          Client client{conn->qp()->qp_num, conn, _state.pd()};
          client._active = true;
          _client_queue.emplace(Operation::CONNECT, msg_t{std::move(client)});
        }

        continue;
      }
      // Allocate structures for connections with an executor.
      // For a connection with a client we don't have to do anything. 
      else if(conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {

        SPDLOG_DEBUG("[Manager-listen] New established connection {} {}", fmt::ptr(conn), conn->private_data());
        continue;
      }
    }
    spdlog::info("Background thread stops waiting for rdmacm events.");
  }

  void Manager::poll_res_mgr()
  {
    // FIXME: add executors!
    typedef std::variant<Client*, ResourceManagerConnection*> conn_t;
    std::unordered_map<uint32_t, conn_t> connections;

    uint32_t id = 0;
    connections[id++] = conn_t{_res_mgr_connection.get()};

    rdmalib::Connection& conn = _res_mgr_connection->_connection.connection();

    //conn.notify_events();

    while(!_shutdown.load()) {

      //auto cq = conn.wait_events();
      //conn.ack_events(cq, 1);

      auto wcs = _res_mgr_connection->_connection.connection().receive_wcs().poll(false);
      if(std::get<1>(wcs)) {

        for(int j = 0; j < std::get<1>(wcs); ++j) {

          auto wc = std::get<0>(wcs)[j];
          if(wc.status != 0)
            continue;
          uint64_t id = wc.wr_id;
          SPDLOG_DEBUG("Receive lease {}", _res_mgr_connection->_receive_buffer[id].lease_id);

          Lease lease {
            _res_mgr_connection->_receive_buffer[id].lease_id,
            _res_mgr_connection->_receive_buffer[id].cores,
            _res_mgr_connection->_receive_buffer[id].memory
          };
          _leases.insert_threadsafe(std::move(lease));
        }
      }

      _res_mgr_connection->_connection.connection().receive_wcs().refill();

      //conn.notify_events();
    }

    spdlog::info("Background thread stops waiting for resource manager events.");
  }

  bool Manager::_process_client(Client & client, uint64_t wr_id)
  {
    int32_t lease_id = client.allocation_requests.data()[wr_id].lease_id;
    char * client_address = client.allocation_requests.data()[wr_id].listen_address;
    int client_port = client.allocation_requests.data()[wr_id].listen_port;

    if(lease_id >= 0) {

      spdlog::info(
        "Client {} requests lease {}, it should connect to {}:{},"
        "it should have buffer of size {}, func buffer {}, and hot timeout {}",
        client.id(), lease_id,
        client.allocation_requests.data()[wr_id].listen_address,
        client.allocation_requests.data()[wr_id].listen_port,
        client.allocation_requests.data()[wr_id].input_buf_size,
        client.allocation_requests.data()[wr_id].func_buf_size,
        client.allocation_requests.data()[wr_id].hot_timeout
      );

      auto lease = _leases.get_threadsafe(lease_id);

      if(!lease.has_value()) {
        spdlog::warn("Received request for unknown lease {}", lease_id);
        *_client_responses.data() = (LeaseStatus) {LeaseStatus::UNKNOWN};
        client.connection->post_send(_client_responses);
        client.connection->receive_wcs().update_requests(-1);
        client.connection->receive_wcs().refill();
        client.connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
        return true;
      }

      rdmalib::PrivateData<0,0,32> data;
      data.secret(client.connection->qp()->qp_num);
      uint64_t addr = client.accounting.address(); //+ sizeof(Accounting)*i;

      // FIXME: Docker
      auto now = std::chrono::high_resolution_clock::now();
      client.executor.reset(
        ProcessExecutor::spawn(
          client.allocation_requests.data()[wr_id],
          _settings.exec,
          {
            _settings.device->ip_address,
            _settings.rdma_device_port,
            data.data(), addr, client.accounting.rkey()
          },
          lease.value()
        )
      );
      auto end = std::chrono::high_resolution_clock::now();
      spdlog::info(
        "Client {} at {}:{} has executor with {} ID and {} cores, time {} us",
        client.id(), client_address, client_port, client.executor->id(), lease->cores,
        std::chrono::duration_cast<std::chrono::microseconds>(end-now).count()
      );

      *_client_responses.data() = (LeaseStatus) {LeaseStatus::ALLOCATED};
      client.connection->post_send(_client_responses);

      client.connection->receive_wcs().update_requests(-1);
      client.connection->receive_wcs().refill();

      client.connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
      return true;
    } else {

      spdlog::info("Client {} disconnects", client.id());
      if(client.executor) {
        auto now = std::chrono::high_resolution_clock::now();
        client.allocation_time +=
          std::chrono::duration_cast<std::chrono::microseconds>(
            now - client.executor->_allocation_finished
          ).count();
      }
      //client.disable(i, _accounting_data.data()[i]);
      client.disable(_res_mgr_connection.get());

      return false;
    }

  }

  void Manager::_check_executors(removals_t & removals)
  {
    for(auto it = _clients.begin(); it != _clients.end(); ++it) {

      Client & client = it->second;
      int i = it->first;
      if(!client.active()) {
        continue;
      }

      if(!client.executor) {
        continue;
      }

      auto status = client.executor->check();
      if(std::get<0>(status) != ActiveExecutor::Status::RUNNING) {
        auto now = std::chrono::high_resolution_clock::now();
        client.allocation_time +=
          std::chrono::duration_cast<std::chrono::microseconds>(
            now - client.executor->_allocation_finished
          ).count();

        // FIXME: update global manager
        // send lease cancellation
        spdlog::info(
          "Executor at client {} exited, status {}, time allocated {} us, polling {} us, execution {} us",
          i, std::get<1>(status), client.allocation_time,
          client.accounting.data()[i].hot_polling_time / 1000.0,
          client.accounting.data()[i].execution_time / 1000.0
        );
        client.executor.reset(nullptr);
        spdlog::info("Finished cleanup");

        // FIXME: notify client
        client.disable(_res_mgr_connection.get());
        removals.push_back(it);
      }

    }
  }

  std::tuple<Manager::Operation, Manager::msg_t>* Manager::_check_queue(int conn_count)
  {
    static std::tuple<Operation, msg_t> result;
    bool updated = false;

    if(conn_count > 0) {
      updated = _client_queue.try_dequeue(result);
    } else {
      updated = _client_queue.wait_dequeue_timed(result, POLLING_TIMEOUT_MS * 1000);
    }

    return updated ? &result : nullptr;
  }

  void Manager::_handle_connections(msg_t & message)
  {
    if(std::holds_alternative<rdmalib::Connection*>(message)) {

      rdmalib::Connection* conn = std::get<rdmalib::Connection*>(message);

      uint32_t qp_num = conn->private_data();
      auto it = _clients.find(qp_num);
      if(it == _clients.end()) {

        SPDLOG_DEBUG("[Manager-RDMA] Rejecting executor to an unknown client {}", qp_num);
        // This operation is thread-safe
        _state.reject(conn);
        delete conn;

      } else {

        SPDLOG_DEBUG("[Manager-RDMA] Accepted a new executor for client {}", qp_num);
        // This operation is thread-safe
        _state.accept(conn);
        (*it).second.executor->add_executor(conn);

      }

    } else {

      Client& client = std::get<Client>(message);
      _state.accept(client.connection);
      _clients.emplace(std::piecewise_construct,
                      std::forward_as_tuple(client.connection->qp()->qp_num),
                      std::forward_as_tuple(std::move(client))
      );
      SPDLOG_DEBUG("[Manager-RDMA] Accepted a new client");

    }
  }

  void Manager::_handle_disconnections(msg_t & message)
  {
    rdmalib::Connection* conn = std::get<rdmalib::Connection*>(message);
    auto it = _clients.find(conn->qp()->qp_num);
    if (it != _clients.end()) {
      spdlog::debug("[Manager] Disconnecting client");
      _clients.erase(it);
    } else {
      spdlog::debug("[Manager] Disconnecting unknown client");
    }
  }

  void Manager::poll_rdma()
  {
    rdmalib::Poller recv_poller{_state.shared_queue(0)};
    int conn_count = 0;
    // FIXME: sleep when there are no clients

    while(!_shutdown.load()) {

      while(true) {

        auto ptr = _check_queue(conn_count);
        if(!ptr) {
          break;
        }

        if (std::get<0>(*ptr) == Operation::CONNECT) {
          _handle_connections(std::get<1>(*ptr));
          conn_count++;
        } else {
          _handle_disconnections(std::get<1>(*ptr));
        }
      }

      std::vector<std::unordered_map<uint32_t, Client>::iterator> removals;
      auto wcs = recv_poller.poll(false);
      for(int j = 0; j < std::get<1>(wcs); ++j) {

        auto wc = std::get<0>(wcs)[j];
        uint32_t qp_num = wc.qp_num;
        auto it = _clients.find(qp_num);
        if(it != _clients.end()) {

          if(wc.status != IBV_WC_SUCCESS) {
            spdlog::error("Failed work completion on client {}, error {}", qp_num, wc.status);
            continue;
          }

          Client & client = (*it).second;
          if(!_process_client(client, wc.wr_id)) {
            removals.push_back(it);
          }

        } else {
          spdlog::error("Polled work completion for QP {}, non-existing client!", qp_num);
        }

      }

      _check_executors(removals);

      if(removals.size()) {
        for(auto it : removals) {
          spdlog::info("Remove client id {}", it->first);
          _clients.erase(it);
        }
      }
    }
    spdlog::info("Background thread stops processing RDMA events.");
    _clients.clear();
  }

}
