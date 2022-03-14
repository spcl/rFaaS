
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "manager.hpp"
#include "rdmalib/connection.hpp"

namespace rfaas::resource_manager {

  constexpr int Manager::POLLING_TIMEOUT_MS;

  Manager::Manager(Settings & settings):
    _executors_output_path(),
    _state(settings.device->ip_address, settings.rdma_device_port,
        settings.device->default_receive_buffer_size, true,
        settings.device->max_inline_data),
    _shutdown(false),
    _http_server(_executor_data, settings)
  {


  }

  void Manager::start()
  {
    // Start HTTP server on a new thread
    _http_server.start();
    spdlog::info("Begin listening and processing events!");
    std::thread listener(&Manager::listen_rdma, this);
    std::thread rdma_poller(&Manager::process_rdma, this);

    listener.join();
    rdma_poller.join();
  }


  void Manager::listen_rdma()
  {
    while(!_shutdown.load()) {
      // Connection initialization:
      // (1) Initialize receive WCs with the allocation request buffer
      bool result = _state.nonblocking_poll_events(POLLING_TIMEOUT_MS);
      if(!result)
        continue;

      auto [conn, conn_status] = _state.poll_events(
        false
      );
      if(conn == nullptr){
        spdlog::error("Failed connection creation");
        continue;
      }
      if(conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {
        // FIXME: handle disconnect
        spdlog::debug("[Manager-listen] Disconnection on connection {}", fmt::ptr(conn));
        continue;
      } else if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
        _state.accept(conn);
      } else if(conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {
        // Accept client connection and push
        SPDLOG_DEBUG("[Manager] Listen thread: connected new client");
        _rdma_queue.enqueue(conn);
      }
    }
    spdlog::info("Background thread stops waiting for rdmacm events");
  }

  void Manager::process_rdma()
  {
    std::vector<rdmalib::Connection*> vec;
    while(!_shutdown.load()) {
      rdmalib::Connection* conn;
      if(_rdma_queue.wait_dequeue_timed(conn, std::chrono::milliseconds(POLLING_TIMEOUT_MS))) {
        spdlog::debug("[Manager] connected new client/executor");
        vec.push_back(conn);
      }
    }

    for(rdmalib::Connection* conn : vec)
      delete conn;

    spdlog::info("Background thread stops processing rdmacm events");
  }

  void Manager::shutdown()
  {
    _shutdown.store(true);
    _http_server.stop();
  }

  void Manager::read_database(const std::string & path)
  {
    _executor_data.read(path);
  }

  void Manager::set_database_path(const std::string & name)
  {
    _executors_output_path = name;
  }

  void Manager::dump_database()
  {
    if(_executors_output_path.has_value()) {
      spdlog::debug("Writing resource manager database to {}", _executors_output_path.value());
      _executor_data.write(_executors_output_path.value());
    }
  }

}
