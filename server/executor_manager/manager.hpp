
#ifndef __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__
#define __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <variant>
#include <vector>
#include <mutex>
#include <map>

#include <readerwriterqueue.h>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>

#include <rfaas/allocation.hpp>

#include "client.hpp"
#include "settings.hpp"
#include "common/messages.hpp"
#include "common.hpp"

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

    void close_lease(int32_t lease_id, uint64_t allocation_time, uint64_t execution_time, uint64_t hot_polling_time)
    {
      *reinterpret_cast<common::LeaseDeallocation*>(_send_buffer.data()) = {
        .lease_id = lease_id,
        .allocation_time = allocation_time,
        .hot_polling_time = hot_polling_time,
        .execution_time = execution_time
      };

      _connection.connection().post_send(_send_buffer, 0);
      auto [wcs, count] = _connection.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
      if(count == 0 || wcs[0].status != IBV_WC_SUCCESS) {
        spdlog::error("Failed to notify resource manager of lease {} close down.", lease_id);
      }
    }

    rdmalib::Connection& connection()
    {
      return _connection.connection();
    }
  };

  struct Lease
  {
    int id;
    int cores;
    int memory;
  };

  struct Leases
  {

    void insert(Lease &&);
    void insert_threadsafe(Lease &&);

    std::optional<Lease> get(int id);
    std::optional<Lease> get_threadsafe(int id);

  private:
    std::unordered_map<int, Lease> _leases;
    std::mutex _mutex;
  };

  struct Manager
  {
    static constexpr int MAX_EXECUTORS_ACTIVE = 8;
    static constexpr int MAX_CLIENTS_ACTIVE = 1024;
    static constexpr int POLLING_TIMEOUT_MS = 100;

    enum class Operation
    {
      CONNECT = 0,
      DISCONNECT = 1
    };

    // The first variatn members corresponds to a new executor for a client.
    // The second one corresponds to a client instance.
    //
    // Integer corresponds to the client number ID.
    //
    typedef std::variant<rdmalib::Connection*, Client> msg_t;
    moodycamel::BlockingReaderWriterQueue<std::tuple<Operation, msg_t>> _client_queue;

    std::mutex clients;
    std::unordered_map<uint32_t, Client> _clients;
    int _ids;

    std::unique_ptr<ResourceManagerConnection> _res_mgr_connection;

    rdmalib::RDMAPassive _state;
    // We could use a circular buffer here if polling for send WCs becomes an issue.
    rdmalib::Buffer<rfaas::LeaseStatus> _client_responses;
    Settings _settings;
    //rdmalib::Buffer<Accounting> _accounting_data;
    bool _skip_rm;
    std::atomic<bool> _shutdown;
    Leases _leases;

    Manager(Settings &, bool skip_rm);

    void start();
    void listen();
    void poll_rdma();
    void poll_res_mgr();
    void shutdown();

  private:

    typedef std::vector<std::unordered_map<uint32_t, Client>::iterator> removals_t;
    void _check_executors(removals_t & removals);
    std::tuple<Operation, msg_t>* _check_queue(bool sleep);
    void _handle_connections(msg_t & message);
    void _handle_disconnections(rdmalib::Connection* conn);
    bool _process_client(Client & client, uint64_t wr_id);
    void _process_events_sleep();
    void _handle_client_message(ibv_wc& wc);
    void _handle_res_mgr_message(ibv_wc& wc);
  };

}

#endif

