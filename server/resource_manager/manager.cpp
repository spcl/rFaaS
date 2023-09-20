

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rfaas/allocation.hpp>

#include "client.hpp"
#include "manager.hpp"

namespace rfaas::resource_manager {

constexpr int Manager::POLLING_TIMEOUT_MS;

Manager::Manager(Settings &settings)
    : _executors_output_path(),
      _state(settings.device->ip_address, settings.rdma_device_port,
             settings.device->default_receive_buffer_size, true,
             settings.device->max_inline_data),
      _shutdown(false),
      _device(*settings.device),
      _http_server(_executor_data, settings),
      _secret(settings.rdma_secret) {}

void Manager::start() {
  // Start HTTP server on a new thread
  _http_server.start();
  spdlog::info("Begin listening and processing events!");
  std::thread listener(&Manager::listen_rdma, this);
  std::thread rdma_poller(&Manager::process_rdma, this);

  listener.join();
  rdma_poller.join();
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
    auto [conn, conn_status] = _state.poll_events(true);
    if (conn == nullptr) {
      spdlog::error("Failed connection creation");
      continue;
    }
    if (conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {
      // FIXME: handle disconnect
      spdlog::debug("[Manager-listen] Disconnection on connection {}",
                    fmt::ptr(conn));
      _rdma_queue.enqueue(std::make_tuple(Operation::DISCONNECT, conn));
    } else if (conn_status == rdmalib::ConnectionStatus::REQUESTED) {

      uint32_t private_data = (*conn).private_data();
      if (private_data && (private_data & 0xFFFF) != this->_secret) {
        spdlog::error("[Manager] Reject executor, wrong secret {}", private_data);
        _state.reject(conn);
      } else {
        _state.accept(conn);
      }
    } else if (conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {
      // Accept client connection and push
      SPDLOG_DEBUG("[Manager] Listen thread: connected new client");
      _rdma_queue.enqueue(std::make_tuple(Operation::CONNECT, conn));
    }
  }
  spdlog::info("Background thread stops waiting for rdmacm events");
}

void Manager::process_rdma() {
  static constexpr int RECV_BUF_SIZE = 64;
  rdmalib::Buffer<rfaas::AllocationRequest> allocation_requests{
      RECV_BUF_SIZE};
  rdmalib::RecvBuffer rcv_buffer{RECV_BUF_SIZE};

  std::vector<rdmalib::Connection *> executors;

  typedef std::unordered_map<uint32_t, Client> client_t;
  client_t clients;
  std::vector<client_t::iterator> removals;
  std::vector<Client*> poll_send;
  int id = 0;

  while (!_shutdown.load()) {

    auto update = _rdma_queue.peek();
    if (update) {

      auto [status, conn] = *update;

      if (status == Operation::CONNECT) {

        uint32_t private_data = (*conn).private_data();
        if (private_data) {

          spdlog::debug("[Manager] connected new executor.");
          // FIXME: allocate atomic memory  for updates
          // FIXME: disconnect executor
          executors.push_back(conn);

        } else {
          spdlog::debug("[Manager] connected new client.");
          clients.emplace(std::piecewise_construct,
                          std::forward_as_tuple((*conn).qp()->qp_num),
                          std::forward_as_tuple(id++, conn, _state.pd()));
        }
      } else {

        uint32_t qp_num = conn->qp()->qp_num;
        auto it = clients.find(qp_num);
        if (it != clients.end()) {
          removals.push_back(it);
          spdlog::debug("[Manager] Disconnecting client {}",
                        it->second.client_id);
        } else {
          spdlog::debug("[Manager] Disconnecting unknown client");
        }
      }

      _rdma_queue.pop();
    }

    // FIXME: sleep
    // FIXME: shared CQS
    // FIXME: poll managers

    for (auto it = clients.begin(); it != clients.end(); ++it) {

      Client &client = std::get<1>(*it);
      int client_id = std::get<0>(*it);
      auto wcs = client.rcv_buffer.poll(false);

      if (std::get<1>(wcs)) {

        for (int j = 0; j < std::get<1>(wcs); ++j) {

          auto wc = std::get<0>(wcs)[j];
          if (wc.status != 0)
            continue;
          uint64_t id = wc.wr_id;
          int16_t cores = client.allocation_requests.data()[id].cores;
          int32_t memory = client.allocation_requests.data()[id].memory;

          if (cores > 0) {
            spdlog::info("Client requests executor with {} threads, it should have {} memory", 
                         client.allocation_requests.data()[id].cores,
                         client.allocation_requests.data()[id].memory
            );

            bool allocated = _executor_data.open_lease(cores, memory, *client.response().data());
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
              client.connection->post_send(
                client.response(),
                0,
                client.response().size() <= _device.max_inline_data,
                1
              );
            }
            poll_send.emplace_back(&client);

          } else {
            spdlog::info("Client {} disconnects", client_id);
            client.disable();
            removals.push_back(it);
            break;
          }
        }
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
        clients.erase(it);
      }
      removals.clear();
    }
  }

  spdlog::info("Background thread stops processing rdmacm events");
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
