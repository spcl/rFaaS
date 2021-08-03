
#ifndef __RFAAS_RESOURCE_MANAGER__
#define __RFAAS_RESOURCE_MANAGER__

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

#include "../common/readerwriterqueue.h"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::resource_manager {

  struct Settings
  {
    std::string rdma_device;
    std::string rdma_device_address;
    int rdma_device_port;
    
    std::string http_network_address;
    int http_network_port;
  };

  struct Options {
    std::string json_config;
    std::string initial_database;
    std::string output_database;
    std::string device_database;
    std::string device;
    bool verbose;
  };
  Options opts(int, char**);

  struct Manager
  {
    moodycamel::ReaderWriterQueue<std::pair<int, std::unique_ptr<rdmalib::Connection>>> _q1;
    moodycamel::ReaderWriterQueue<std::pair<int,Client>> _q2;


    rdmalib::RDMAPassive _state;
    // FIXME: multicast

    std::mutex clients;
    std::map<int, Client> _clients;
    int _ids;

    //std::vector<Client> _clients;
    //std::atomic<int> _clients_active;
    rdmalib::server::ServerStatus _status;
    ExecutorSettings _settings;
    //rdmalib::Buffer<Accounting> _accounting_data;
    std::string _address;
    int _port;
    int _secret;

    Manager(std::string addr, int port, std::string server_file, const ExecutorSettings & settings);

    void start();
    void listen();
    void poll_rdma();
  };

}

#endif

