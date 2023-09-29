

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/queue.hpp>
#include <rfaas/allocation.hpp>

#include "client.hpp"
#include "common/messages.hpp"
#include "manager.hpp"
#include "executor.hpp"
#include "rdmalib/poller.hpp"

namespace rfaas::resource_manager {

constexpr int Manager::POLLING_TIMEOUT_MS;

Manager::Manager(Settings &settings)
    : _executors_output_path(),
      _state(settings.device->ip_address, settings.rdma_device_port,
             settings.device->default_receive_buffer_size, true,
             settings.device->max_inline_data),
      _shutdown(false),
      _device(*settings.device),
      _executors(_state.pd()),
      _executor_data(_executors),
      _http_server(_executor_data, settings),
      _secret(settings.rdma_secret) {}

void Manager::start() {
  // Start HTTP server on a new thread
  _http_server.start();
  spdlog::info("Begin listening and processing events!");

  _state.register_shared_queue(1);
  _state.register_shared_queue(2);

  std::thread listener(&Manager::listen_rdma, this);
  std::thread rdma_poller(&Manager::process_clients, this);
  std::thread exec_poller(&Manager::process_executors, this);

  listener.join();
  rdma_poller.join();
  exec_poller.join();
}

void Manager::listen_rdma() {

  while (!_shutdown.load()) {
    // Connection initialization:
    // (1) Initialize receive WCs with the allocation request buffer
    bool result = _state.nonblocking_poll_events(POLLING_TIMEOUT_MS);
    if (!result)
      continue;

    // We share the CQS to ensure that all clients have the same receiving
    // queue. This way, we can poll events on a single connection.
    auto [conn, conn_status] = _state.poll_events();
    if (conn == nullptr) {
      spdlog::error("Failed connection creation");
      continue;
    }
    rdmalib::PrivateData private_data{conn->private_data()};

    if (conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {

      // FIXME: handle disconnect
      spdlog::debug("[Manager-listen] Disconnection on connection {}",
                    fmt::ptr(conn));

      // FIXME: reenable
      //_rdma_queue.enqueue(std::make_tuple(Operation::DISCONNECT, conn));
      if (private_data.key() == 1) {
        _executor_queue.enqueue(std::make_tuple(Operation::DISCONNECT, conn));
      } else {
        _client_queue.enqueue(std::make_tuple(Operation::DISCONNECT, conn));
      }

    } else if (conn_status == rdmalib::ConnectionStatus::REQUESTED) {

      if (private_data.key() == 1 && private_data.secret() != this->_secret) {

        uint32_t private_data = (*conn).private_data();
        spdlog::error("[Manager] Reject executor, wrong secret {}", private_data);
        _state.reject(conn);

      } else if (private_data.key() == 1) {

        _executors.connect_executor(conn);
        _state.accept(conn);

      } else {

        _state.accept(conn);
      }

    } else if (conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {

      if (private_data.key() == 1) {
        SPDLOG_DEBUG("[Manager] Listen thread: connected new executor");
        _executor_queue.enqueue(std::make_tuple(Operation::CONNECT, conn));
      } else if (private_data.key() == 2) {
        SPDLOG_DEBUG("[Manager] Listen thread: connected new client");
        _client_queue.enqueue(std::make_tuple(Operation::CONNECT, conn));
      } else {
        SPDLOG_DEBUG("[Manager] Listen thread: unknown connection!");
        conn->close();
      }

    }
  }
  spdlog::info("Background thread stops waiting for rdmacm events");
}

void Manager::_handle_message(int qp_num, int msg_num)
{
  auto buf = &_executors.get_executor(qp_num)->_receive_buffer[msg_num * Executor::MSG_SIZE];

  auto type = *reinterpret_cast<uint32_t*>(buf);
  if(type == common::id_to_int(common::MessageIDs::NODE_REGISTRATION)) {

    auto ptr = reinterpret_cast<common::NodeRegistration*>(buf);
    _executors.register_executor(qp_num, ptr->node_name);

  } else if(type == common::id_to_int(common::MessageIDs::LEASE_DEALLOCATION)) {

    auto ptr = reinterpret_cast<common::LeaseDeallocation*>(buf);
    _executor_data.close_lease(*ptr);

  } else {
    spdlog::error("Unknown message from executor! Unknown message type {}", type);
  }

}

void Manager::process_executors()
{
  int executor_count = 0;
  rdmalib::RecvWorkCompletions* recv_queue = nullptr;
  typedef std::unordered_map<uint32_t, rdmalib::RecvWorkCompletions*> exec_t;
  exec_t executors;

  std::vector<exec_t::iterator> removals;
  // FIXME: reenable
  //std::vector<Executor*> poll_send;

  while (!_shutdown.load()) {

    bool updated = false;

    std::tuple<Operation, rdmalib::Connection*> result;
    std::tuple<Operation, rdmalib::Connection*>* result_ptr;
    if(executor_count > 0) {

      result_ptr = _executor_queue.peek();
      if(result_ptr) {
        updated = true;
      }

    } else {
      updated = _executor_queue.wait_dequeue_timed(result, POLLING_TIMEOUT_MS * 1000);
      if(updated) {
        result_ptr = &result;
      }
    }

    if (updated) {

      if (std::get<0>(*result_ptr) == Operation::CONNECT) {

        rdmalib::Connection* conn = std::get<1>(*result_ptr);
        executors.emplace(std::piecewise_construct,
                        std::forward_as_tuple((*conn).qp()->qp_num),
                        std::forward_as_tuple(&conn->receive_wcs()));

        if(!recv_queue) {
          recv_queue = &conn->receive_wcs();
        }
        
      } else {

        // FIXME: remove executor data
        rdmalib::Connection* conn = std::get<1>(*result_ptr);
        executors.erase(conn->qp()->qp_num);
      }

      if(executor_count > 0) {
        _executor_queue.pop();
      }

      executor_count++;
    }

    // FIXME: sleep
    // FIXME: poll managers
 
    if(executor_count > 0) {

      auto wcs = recv_queue->poll(false);
      if(std::get<1>(wcs)) {

        for (int j = 0; j < std::get<1>(wcs); ++j) {

          auto wc = std::get<0>(wcs)[j];
          uint64_t id = wc.wr_id;
          uint32_t qp_num = wc.qp_num;

          _handle_message(qp_num, id);

          if(wc.qp_num != recv_queue->qp()->qp_num) {
            executors[wc.qp_num]->update_requests(-1);
            executors[wc.qp_num]->refill();
            recv_queue->update_requests(1);
          } else {
            recv_queue->refill();
          }
        }

      }
    }

    // FIXME: reenable
    //if (poll_send.size()) {
    //  for (auto client : poll_send) {
    //    client->connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
    //  }
    //  poll_send.clear();
    //}

    if (removals.size()) {
      for (auto it : removals) {
        //spdlog::info("Remove client id {}", it->second.client_id);
        executors.erase(it);
      }
      removals.clear();
    }
  }

  spdlog::info("Background thread stops processing rdmacm events");
}

void Manager::process_clients() {

  rdmalib::Poller recv_poller;
  typedef std::unordered_map<uint32_t, Client> client_t;
  client_t clients;
  std::vector<client_t::iterator> removals;
  std::vector<Client*> poll_send;
  int client_count = 0;
  int id = 0;


  // FIXME: reenable
  //std::vector<Executor*> poll_send;

  while (!_shutdown.load()) {

    bool updated = false;

    std::tuple<Operation, rdmalib::Connection*> result;
    std::tuple<Operation, rdmalib::Connection*>* result_ptr;
    if(client_count > 0) {

      result_ptr = _client_queue.peek();
      if(result_ptr) {
        updated = true;
      }

    } else {
      updated = _client_queue.wait_dequeue_timed(result, POLLING_TIMEOUT_MS * 1000);
      if(updated) {
        result_ptr = &result;
      }
    }

    if (updated) {

      if (std::get<0>(*result_ptr) == Operation::CONNECT) {

        rdmalib::Connection* conn = std::get<1>(*result_ptr);
        clients.emplace(
          std::piecewise_construct,
          std::forward_as_tuple((*conn).qp()->qp_num),
          std::forward_as_tuple(id++, conn, _state.pd())
        );
        spdlog::debug("[Manager] Connecting client {}", id - 1);

        if(!recv_poller.initialized()) {
          recv_poller.initialize(conn->qp()->recv_cq);
        }

      } else {

        rdmalib::Connection* conn = std::get<1>(*result_ptr);
        auto it = clients.find(conn->qp()->qp_num);
        if (it != clients.end()) {
          removals.push_back(it);
          spdlog::debug("[Manager] Disconnecting client {}",
                        it->second.client_id);
        } else {
          spdlog::debug("[Manager] Disconnecting unknown client");
        }
      }

      if(client_count > 0) {
        _client_queue.pop();
      }

      if (std::get<0>(*result_ptr) == Operation::CONNECT) {
        client_count++;
      }
    }

    // FIXME: sleep
    auto wcs = recv_poller.poll(false);
    for (int j = 0; j < std::get<1>(wcs); ++j) {

      auto wc = std::get<0>(wcs)[j];
      uint64_t id = wc.wr_id;
      uint32_t qp_num = wc.qp_num;

      auto it = clients.find(qp_num);
      if(it == clients.end()) {
        spdlog::warn("Polled work completion for QP {}, non-existing client!", qp_num);
        continue;
      }

      if(wc.status != IBV_WC_SUCCESS) {
        spdlog::error("Failed work completion on client {}, error {}", qp_num, wc.status);
        continue;
      }

      Client& client = (*it).second;

      int16_t cores = client.allocation_requests.data()[id].cores;
      int32_t memory = client.allocation_requests.data()[id].memory;

      if (cores > 0) {
        spdlog::info("Client requests executor with {} threads, it should have {} memory", 
                    client.allocation_requests.data()[id].cores,
                    client.allocation_requests.data()[id].memory
        );

        auto allocated = _executor_data.open_lease(cores, memory, *client.response().data());
        if(allocated) {
          spdlog::info("[Manager] Client receives lease with id {}", client.response().data()->lease_id);
        } else {
          spdlog::info("[Manager] Client request couldn't be satisfied");
        }

        if(!allocated) {
          // FIXME: Send empty response?
          client.connection->post_send(
            client.response(),
            0,
            client.response().size() <= _device.max_inline_data,
            0
          );
        } else {

          allocated->_send_buffer[0].lease_id = client.response()[0].lease_id;
          allocated->_send_buffer[0].cores = cores;
          allocated->_send_buffer[0].memory = memory;

          allocated->_connection->post_send(
            allocated->_send_buffer,
            0,
            allocated->_send_buffer.size() <= _device.max_inline_data,
            1
          );
          allocated->_connection->poll_wc(rdmalib::QueueType::SEND, true, 1);

          client.connection->post_send(
            client.response(),
            0,
            client.response().size() <= _device.max_inline_data,
            1
          );
        }
        poll_send.emplace_back(&client);

      } else {
        spdlog::info("Client {} disconnects", client.client_id);
        client.disable();
        removals.push_back(it);
        break;
      }

      client.connection->receive_wcs().update_requests(-1);
      client.connection->receive_wcs().refill();
    }

    if (poll_send.size()) {
      for (auto client : poll_send) {
        client->connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
      }
      poll_send.clear();
    }

    if (removals.size()) {
      for (auto it : removals) {
        spdlog::info("Remove client id {}", it->second.client_id);
        clients.erase(it);
      }

      //client_count -= removals.size();
      //if(client_count == 0) {
      //  recv_poller.initialize(nullptr);
      //}

      removals.clear();
    }
  }

  spdlog::info("Background thread stops processing client events");

  //while (!_shutdown.load()) {

  //  auto update = _rdma_queue.peek();
  //  if (update) {

  //    auto [status, conn] = *update;

  //    if (status == Operation::CONNECT) {

  //      uint32_t private_data = (*conn).private_data();
  //      if (private_data) {

  //        spdlog::debug("[Manager] connected new executor.");
  //        // FIXME: allocate atomic memory  for updates
  //        // FIXME: executor registration
  //        _executors.connect_executor(conn);

  //      } else {
  //        spdlog::debug("[Manager] connected new client.");
  //        clients.emplace(std::piecewise_construct,
  //                        std::forward_as_tuple((*conn).qp()->qp_num),
  //                        std::forward_as_tuple(id++, conn, _state.pd()));
  //      }
  //    } else {

  //      // FIXME: disconnect executor
  //      uint32_t qp_num = conn->qp()->qp_num;
  //      auto it = clients.find(qp_num);
  //      if (it != clients.end()) {
  //        removals.push_back(it);
  //        spdlog::debug("[Manager] Disconnecting client {}",
  //                      it->second.client_id);
  //      } else {
  //        spdlog::debug("[Manager] Disconnecting unknown client");
  //      }
  //    }

  //    _rdma_queue.pop();
  //  }

  //  // FIXME: sleep
  //  // FIXME: shared CQS
  //  // FIXME: poll managers
 
  //  // FIXME: abstraction!
  //  auto it = _executors._unregistered_executors.begin();

  //  if(it != _executors._unregistered_executors.end()) {
  //    auto wcs = (*it).second->poll_wc(rdmalib::QueueType::RECV, false, 1);
  //    if(std::get<1>(wcs)) {

  //      spdlog::info("Polled!");

  //      for (int j = 0; j < std::get<1>(wcs); ++j) {

  //        auto wc = std::get<0>(wcs)[j];
  //        uint64_t id = wc.wr_id;
  //        // FIXME: parse message
  //        spdlog::error("{} {}", fmt::ptr(_executors._receive_buffer.data()), id * sizeof(Executors::MSG_SIZE));
  //        spdlog::error("{} {}", fmt::ptr(&_executors._receive_buffer[id * sizeof(Executors::MSG_SIZE)]), id * sizeof(Executors::MSG_SIZE));
  //        auto ptr = reinterpret_cast<common::NodeRegistration*>(&_executors._receive_buffer[id * sizeof(Executors::MSG_SIZE)]);
  //        spdlog::error("{}", fmt::ptr(ptr));
  //        std::cerr << strlen(ptr->node_name) << std::endl;
  //      }
  //    }
  //  }

  //  for (auto it = clients.begin(); it != clients.end(); ++it) {

  //    Client &client = std::get<1>(*it);
  //    int client_id = std::get<0>(*it);
  //    auto wcs = client.rcv_buffer.poll(false);

  //    if (std::get<1>(wcs)) {

  //      for (int j = 0; j < std::get<1>(wcs); ++j) {

  //        auto wc = std::get<0>(wcs)[j];
  //        if (wc.status != 0)
  //          continue;
  //        uint64_t id = wc.wr_id;
  //        int16_t cores = client.allocation_requests.data()[id].cores;
  //        int32_t memory = client.allocation_requests.data()[id].memory;

  //        if (cores > 0) {
  //          spdlog::info("Client requests executor with {} threads, it should have {} memory", 
  //                       client.allocation_requests.data()[id].cores,
  //                       client.allocation_requests.data()[id].memory
  //          );

  //          bool allocated = _executor_data.open_lease(cores, memory, *client.response().data());
  //          if(allocated) {
  //            spdlog::info("[Manager] Client receives lease with id {}", client.response().data()->lease_id);
  //          } else {
  //            spdlog::info("[Manager] Client request couldn't be satisfied");
  //          }

  //          if(!allocated) {
  //            // FIXME: Send empty response?
  //            client.connection->post_send(
  //              client.response(),
  //              0,
  //              client.response().size() <= _device.max_inline_data,
  //              0
  //            );
  //          } else {
  //            client.connection->post_send(
  //              client.response(),
  //              0,
  //              client.response().size() <= _device.max_inline_data,
  //              1
  //            );
  //          }
  //          poll_send.emplace_back(&client);

  //        } else {
  //          spdlog::info("Client {} disconnects", client_id);
  //          client.disable();
  //          removals.push_back(it);
  //          break;
  //        }
  //      }
  //    }
  //  }

  //  if (poll_send.size()) {
  //    for (auto client : poll_send) {
  //      client->connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
  //    }
  //    poll_send.clear();
  //  }

  //  if (removals.size()) {
  //    for (auto it : removals) {
  //      spdlog::info("Remove client id {}", it->second.client_id);
  //      clients.erase(it);
  //    }
  //    removals.clear();
  //  }
  //}
}

void Manager::shutdown() {
  _shutdown.store(true);
  _http_server.stop();
}

void Manager::read_database(const std::string &path) {
  _executor_data.read(path);
}

void Manager::set_database_path(const std::string &name) {
  _executors_output_path = name;
}

void Manager::dump_database() {
  if (_executors_output_path.has_value()) {
    spdlog::debug("Writing resource manager database to {}",
                  _executors_output_path.value());
    _executor_data.write(_executors_output_path.value());
  }
}

} // namespace rfaas::resource_manager
