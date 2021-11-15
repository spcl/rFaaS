
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
#include <rdmalib/recv_buffer.hpp>

#include "client.hpp"
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

  struct Manager
  {
    // FIXME: we need a proper data structure that is thread-safe and scales
    //static constexpr int MAX_CLIENTS_ACTIVE = 128;
    static constexpr int MAX_EXECUTORS_ACTIVE = 8;
    static constexpr int MAX_CLIENTS_ACTIVE = 1024;
    static constexpr int POLLING_TIMEOUT_MS = 100;
    moodycamel::ReaderWriterQueue<std::pair<int, rdmalib::Connection*>> _q1;
    moodycamel::ReaderWriterQueue<std::pair<int, Client>> _q2;

    std::mutex clients;
    std::map<int, Client> _clients;
    int _ids;

    //std::vector<Client> _clients;
    //std::atomic<int> _clients_active;
    rdmalib::RDMAActive _res_mgr_connection;
    //std::unique_ptr<rdmalib::Connection> _res_mgr_connection;

    rdmalib::RDMAPassive _state;
    //rdmalib::server::ServerStatus _status;
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

