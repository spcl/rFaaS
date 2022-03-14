
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
    //moodycamel::ReaderWriterQueue<std::pair<int,Client>> _q2;
    //std::mutex clients;
    //std::map<int, Client> _clients;
    //int _ids;

    ExecutorDB _executor_data;
    std::optional<std::string> _executors_output_path;

    // Handling RDMA connections with clients and executor managers
    moodycamel::BlockingReaderWriterQueue<
      rdmalib::Connection*
    > _rdma_queue;
    rdmalib::RDMAPassive _state;
    std::atomic<bool> _shutdown;

    // Handling HTTP events
    HTTPServer _http_server;

    // configuration parameters
    Settings _settings;
    static constexpr int POLLING_TIMEOUT_MS = 100;

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

