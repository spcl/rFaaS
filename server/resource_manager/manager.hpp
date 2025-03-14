
#ifndef __RFAAS_RESOURCE_MANAGER__
#define __RFAAS_RESOURCE_MANAGER__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <variant>
#include <vector>
#include <mutex>
#include <map>
#include <optional>

#include <readerwriterqueue.h>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>

#include <rfaas/devices.hpp>

#include "client.hpp"
#include "db.hpp"
#include "http.hpp"
#include "settings.hpp"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::resource_manager {

  struct Options {
    std::string json_config;
    std::string initial_database;
    std::string output_database;
    std::string device_database;
    bool verbose;
  };
  Options opts(int, char**);

  struct Manager
  {
    std::optional<std::string> _executors_output_path;

    // Handling RDMA connections with clients and executor managers
    enum class Operation
    {
      CONNECT = 0,
      DISCONNECT = 1
    };

    typedef std::variant<rdmalib::Connection*, Client> client_msg_t;
    typedef moodycamel::BlockingReaderWriterQueue<
      std::tuple<Operation, client_msg_t>
    > client_queue_t;

    client_queue_t _client_queue;

    typedef std::variant<rdmalib::Connection*, std::shared_ptr<Executor>> exec_msg_t;
    typedef moodycamel::BlockingReaderWriterQueue<
      std::tuple<Operation, exec_msg_t>
    > executor_queue_t;
    executor_queue_t _executor_queue;

    typedef std::unordered_map<uint32_t, Client> client_t;
    client_t _clients;
    int _client_id;

    rdmalib::RDMAPassive _state;
    std::atomic<bool> _shutdown;
    rfaas::device_data _device;

    Executors _executors;
    ExecutorDB _executor_data;

    // Handling HTTP events
    HTTPServer _http_server;

    // configuration parameters
    Settings _settings;
    static constexpr int POLLING_TIMEOUT_MS = 100;

    uint32_t _secret;

    Manager(Settings &);

    void read_database(const std::string & name);
    void set_database_path(const std::string & name);
    void dump_database();
    void start();
    void shutdown();

    void listen_rdma();
    void process_clients();
    void process_executors();
    void process_events_sleep();
  private:
    void _handle_message(ibv_wc& wc);
    std::tuple<Manager::Operation, exec_msg_t>* _check_queue(executor_queue_t& queue, bool sleep);
    std::tuple<Manager::Operation, client_msg_t>* _check_queue(client_queue_t& queue, bool sleep);

    void _handle_executor_disconnection(rdmalib::Connection* conn);
    void _handle_executor_connection(std::shared_ptr<Executor> && exec);

    void _handle_client_message(ibv_wc& wc, std::vector<Client*>& poll_send);
    void _handle_client_connection(Client & client);
    void _handle_client_disconnection(rdmalib::Connection* conn);
  };

}

#endif

