


#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>
#include <sys/poll.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/queue.hpp>
#include <rfaas/allocation.hpp>

#include "common/messages.hpp"
#include "manager.hpp"
#include "executor.hpp"
#include "rdmalib/poller.hpp"

namespace rfaas::resource_manager {

constexpr int Manager::POLLING_TIMEOUT_MS;

Manager::Manager(Settings &settings):
    _executors_output_path(),
    _client_id(0),
    _state(settings.device->ip_address, settings.rdma_device_port,
            settings.device->default_receive_buffer_size, true,
            settings.device->max_inline_data),
    _shutdown(false),
    _device(*settings.device),
    _executors(_state.pd()),
    _executor_data(_executors),
    _http_server(_executor_data, settings),
    _settings(settings),
    _secret(settings.rdma_secret)
  {}

void Manager::start() {
  // Start HTTP server on a new thread
  _http_server.start();
  spdlog::info("Begin listening and processing events!");

  _state.register_shared_queue(1);
  _state.register_shared_queue(2);

  std::thread listener(&Manager::listen_rdma, this);

  if(_settings.rdma_sleep) {

    std::thread rdma_processer(&Manager::process_events_sleep, this);
    rdma_processer.join();

  } else {

    std::thread rdma_poller(&Manager::process_clients, this);
    std::thread exec_poller(&Manager::process_executors, this);

    rdma_poller.join();
    exec_poller.join();

  }
  listener.join();
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

      spdlog::debug("[Manager-listen] Disconnection on connection {}",
                    fmt::ptr(conn));

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

        auto exec = std::make_shared<Executor>();
        exec->initialize_connection(_state.pd(), conn);

        _executor_queue.enqueue(
            std::make_tuple(
              Operation::CONNECT,
              exec_msg_t{std::move(exec)}
            )
        );
        _state.accept(conn);

      } else {

        _client_queue.enqueue(
            std::make_tuple(
              Operation::CONNECT,
              client_msg_t{Client{_client_id++, conn, _state.pd()}}
            )
        );
        _state.accept(conn);
      }

    } else if (conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {

      if (private_data.key() == 1) {
        SPDLOG_DEBUG("[Manager] Listen thread: connected new executor");
      } else if (private_data.key() == 2) {
        SPDLOG_DEBUG("[Manager] Listen thread: connected new client");
      } else {
        SPDLOG_DEBUG("[Manager] Listen thread: unknown connection!");
        conn->close();
      }

    }
  }
  spdlog::info("Background thread stops waiting for rdmacm events");
}

void Manager::_handle_message(ibv_wc& wc)
{

  if(wc.status != IBV_WC_SUCCESS) {
    spdlog::error("Failed work completion on client {}, error {}", wc.qp_num, wc.status);
    return;
  }

  uint64_t id = wc.wr_id;
  uint32_t qp_num = wc.qp_num;
  auto exec = _executors.get_executor(qp_num);

  if(!exec) {
    spdlog::error("Polled work completion for QP {}, non-existing executor!", qp_num);
    return;
  }

  auto buf = &exec->_receive_buffer[id * Executor::MSG_SIZE];

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

  exec->_connection->receive_wcs().update_requests(-1);
  exec->_connection->receive_wcs().refill();
}

std::tuple<Manager::Operation, Manager::exec_msg_t>* Manager::_check_queue(executor_queue_t& queue, bool sleep)
{
  static std::tuple<Operation, exec_msg_t> result;
  bool updated = false;

  if(!sleep) {
    updated = queue.try_dequeue(result);
  } else {
    updated = queue.wait_dequeue_timed(result, POLLING_TIMEOUT_MS * 1000);
  }

  return updated ? &result : nullptr;
}

std::tuple<Manager::Operation, Manager::client_msg_t>* Manager::_check_queue(client_queue_t& queue, bool sleep)
{
  static std::tuple<Operation, client_msg_t> result;
  bool updated = false;

  if(!sleep) {
    updated = queue.try_dequeue(result);
  } else {
    updated = queue.wait_dequeue_timed(result, POLLING_TIMEOUT_MS * 1000);
  }

  return updated ? &result : nullptr;
}

void Manager::_handle_executor_connection(std::shared_ptr<Executor> && exec)
{
  _executors.connect_executor(std::move(exec));
}

void Manager::_handle_executor_disconnection(rdmalib::Connection* conn)
{
  _executors.remove_executor(conn->qp()->qp_num);
}

void Manager::process_executors()
{
  rdmalib::Poller recv_poller{std::get<1>(*_state.shared_queue(1))};
  int executor_count = 0;

  while (!_shutdown.load()) {

    while(true) {

      auto ptr = _check_queue(_executor_queue, executor_count == 0);
      if(!ptr) {
        break;
      }

      if (std::get<0>(*ptr) == Operation::CONNECT) {
        _handle_executor_connection(
          std::move(std::get<1>(std::get<1>(*ptr)))
        );
        executor_count++;
      } else {
        _handle_executor_disconnection(
          std::get<0>(std::get<1>(*ptr))
        );
        executor_count--;
      }
    }

    auto wcs = recv_poller.poll(false);
    if(std::get<1>(wcs)) {
      for (int j = 0; j < std::get<1>(wcs); ++j) {
        _handle_message(std::get<0>(wcs)[j]);
      }
    }

  }

  spdlog::info("Background thread stops processing rdmacm events");
}

void Manager::_handle_client_connection(Client& client)
{
  uint32_t qp_num = client.connection->qp()->qp_num;
  _clients.insert_or_assign(qp_num, std::move(client));
  spdlog::debug("[Manager] Connecting client {}", _client_id - 1);
}

void Manager::_handle_client_disconnection(rdmalib::Connection* conn)
{
  _executors.remove_executor(conn->qp()->qp_num);
  auto it = _clients.find(conn->qp()->qp_num);
  if (it != _clients.end()) {
    _clients.erase(it);
    spdlog::debug("[Manager] Disconnecting client {}",
                  it->second.client_id);
  } else {
    spdlog::debug("[Manager] Disconnecting unknown client");
  }
}

void Manager::_handle_client_message(ibv_wc& wc, std::vector<Client*>& poll_send)
{

  if(wc.status != IBV_WC_SUCCESS) {
    spdlog::error("Failed work completion on client {}, error {}", wc.qp_num, wc.status);
    return;
  }

  uint64_t id = wc.wr_id;
  uint32_t qp_num = wc.qp_num;

  auto it = _clients.find(qp_num);
  if(it == _clients.end()) {
    spdlog::warn("Polled work completion for QP {}, non-existing client!", qp_num);
    return;
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

    client.connection->receive_wcs().update_requests(-1);
    client.connection->receive_wcs().refill();

  } else {
    spdlog::info("Client {} disconnects", client.client_id);
    client.disable();
    _clients.erase(it);
  }

  return;
}

void Manager::process_clients()
{
  rdmalib::Poller recv_poller{std::get<1>(*_state.shared_queue(2))};
  int client_count = 0;
  std::vector<Client*> poll_send;
  std::vector<client_t::iterator> removals;

  while (!_shutdown.load()) {

    while(true) {

      auto ptr = _check_queue(_client_queue, client_count == 0);
      if(!ptr) {
        break;
      }

      if (std::get<0>(*ptr) == Operation::CONNECT) {
        _handle_client_connection(std::get<1>(std::get<1>(*ptr)));
        client_count++;
      } else {
        _handle_client_disconnection(std::get<0>(std::get<1>(*ptr)));
        client_count--;
      }
    }

    auto wcs = recv_poller.poll(false);
    if(std::get<1>(wcs)) {
      for (int j = 0; j < std::get<1>(wcs); ++j) {
        _handle_client_message(std::get<0>(wcs)[j], poll_send);
      }
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
        _clients.erase(it);
      }

      client_count -= removals.size();
      removals.clear();
    }

  }

  spdlog::info("Background thread stops processing client events");
}

void Manager::process_events_sleep()
{
  rdmalib::EventPoller event_poller;

  auto [client_channel, client_cq, _] = *_state.shared_queue(2);
  rdmalib::Poller client_poller{client_cq};
  client_poller.set_nonblocking();
  client_poller.notify_events(false);

  ibv_comp_channel* executor_channel;
  ibv_cq* executor_cq;
  std::tie(executor_channel, executor_cq, std::ignore) = *_state.shared_queue(1);
  rdmalib::Poller executor_poller{executor_cq};
  executor_poller.set_nonblocking();
  executor_poller.notify_events(false);

  event_poller.add_channel(client_poller, 2);
  event_poller.add_channel(executor_poller, 1);

  std::vector<Client*> poll_send;

  auto queue_client = [this]() {

    while(true) {

      auto ptr = _check_queue(_client_queue, false);
      if(!ptr) {
        break;
      }

      if(ptr) {
        if (std::get<0>(*ptr) == Operation::CONNECT) {
          _handle_client_connection(std::get<1>(std::get<1>(*ptr)));
        } else {
          _handle_client_disconnection(std::get<0>(std::get<1>(*ptr)));
        }
      }

    }

  };

  auto queue_executor = [this]() {

    while(true) {

      auto ptr_exec = _check_queue(_executor_queue, false);
      if(!ptr_exec) {
        break;
      }

      if(ptr_exec) {
        if (std::get<0>(*ptr_exec) == Operation::CONNECT) {
          _handle_executor_connection(std::move(std::get<1>(std::get<1>(*ptr_exec))));
        } else {
          _handle_executor_disconnection(std::get<0>(std::get<1>(*ptr_exec)));
        }
      }

    }

  };

  while (!_shutdown.load()) {

    auto [events, count] = event_poller.poll(POLLING_TIMEOUT_MS*10);

    for(int i = 0; i < count; ++i) {

      if(events[i].data.u32 == 2) {

        queue_client();

        auto cq = client_poller.wait_events();
        client_poller.ack_events(cq, 1);
        client_poller.notify_events(false) ;

        auto wcs = client_poller.poll(false);
        if(std::get<1>(wcs)) {
          for (int j = 0; j < std::get<1>(wcs); ++j) {
            _handle_client_message(std::get<0>(wcs)[j], poll_send);
          }
        }

      } else {

        queue_executor();

        auto cq = executor_poller.wait_events();
        executor_poller.ack_events(cq, 1);
        executor_poller.notify_events(false);

        auto wcs = executor_poller.poll(false);
        if(std::get<1>(wcs)) {
          for (int j = 0; j < std::get<1>(wcs); ++j) {
            _handle_message(std::get<0>(wcs)[j]);
          }
        }

      }

    }

    queue_client();
    queue_executor();

    if (poll_send.size()) {
      for (auto client : poll_send) {
        client->connection->poll_wc(rdmalib::QueueType::SEND, true, 1);
      }
      poll_send.clear();
    }

  }
  spdlog::info("Background thread stops processing client events");
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
