
#ifndef __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__
#define __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>
#include <mutex>
#include <map>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>

#include "client.hpp"
#include "common/messages.hpp"
#include "settings.hpp"
#include "../common.hpp"
#include "../common/readerwriterqueue.h"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::executor_manager {

  struct Options {
    std::string json_config;
    std::string device_database;
    bool skip_rm;
    bool verbose;
  };
  Options opts(int, char**);

  struct ResourceManagerConnection
  {
    rdmalib::RDMAActive _connection;
    rdmalib::Buffer<common::LeaseAllocation>  _receive_buffer;
    rdmalib::Buffer<uint8_t>  _send_buffer;

    ResourceManagerConnection(const std::string& name, int port, int receive_buf_size):
      _connection(name, port, receive_buf_size),
      _receive_buffer(receive_buf_size),
      _send_buffer(std::max(sizeof(common::LeaseDeallocation), sizeof(common::NodeRegistration)))
    {
      _connection.allocate();
      _send_buffer.register_memory(_connection.pd(), IBV_ACCESS_LOCAL_WRITE); 
      _receive_buffer.register_memory(_connection.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE); 
    }

    bool connect(const std::string& node_name, uint32_t resource_manager_secret)
    {
      if(!_connection.connect(resource_manager_secret)) {
        spdlog::error("Connection to resource manager was not succesful!");
        return false;
      }

      _connection.connection().receive_wcs().initialize(_receive_buffer);

      common::NodeRegistration reg;
      strncpy(reg.node_name, node_name.c_str(), common::NodeRegistration::NODE_NAME_LENGTH);
      memcpy(_send_buffer.data(), &reg, sizeof(common::NodeRegistration));

      _connection.connection().post_send(_send_buffer, 0);
      _connection.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);

      return true;
    }
  };

  struct Manager
  {
    static constexpr int MAX_EXECUTORS_ACTIVE = 8;
    static constexpr int MAX_CLIENTS_ACTIVE = 1024;
    static constexpr int POLLING_TIMEOUT_MS = 100;
    moodycamel::ReaderWriterQueue<std::pair<int, rdmalib::Connection*>> _q1;
    moodycamel::ReaderWriterQueue<std::pair<int, Client>> _q2;

    std::mutex clients;
    std::map<int, Client> _clients;
    int _ids;

    std::unique_ptr<ResourceManagerConnection> _res_mgr_connection;

    rdmalib::RDMAPassive _state;
    Settings _settings;
    //rdmalib::Buffer<Accounting> _accounting_data;
    uint32_t _secret;
    bool _skip_rm;
    std::atomic<bool> _shutdown;

    Manager(Settings &, bool skip_rm);

    void start();
    void listen();
    void poll_rdma();
    void shutdown();
  };

}

#endif

