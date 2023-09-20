
#ifndef __RFAAS_RESOURCE_MANAGER__
#define __RFAAS_RESOURCE_MANAGER__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>
#include <mutex>
#include <map>
#include <optional>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

#include <rfaas/devices.hpp>

#include "common/readerwriterqueue.h"
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

    moodycamel::BlockingReaderWriterQueue<
      std::tuple<Operation, rdmalib::Connection*>
    > _rdma_queue;
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
    void process_rdma();
  };

}

#endif

