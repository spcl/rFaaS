
#ifndef __RFAAS_RESOURCE_MANAGER_DB_HPP__
#define __RFAAS_RESOURCE_MANAGER_DB_HPP__

#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include <rfaas/allocation.hpp>
#include <rfaas/resources.hpp>

#include <rdmalib/buffer.hpp>

namespace rfaas { namespace resource_manager {

  struct node_data : server_data
  {
    int free_cores;
    int free_memory;

    node_data(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory):
      server_data(node_name, ip, port, cores, memory),
      free_cores(cores),
      free_memory(memory)
    {}
  };

  struct ExecutorDB
  {
  private:
    typedef std::shared_lock<std::shared_mutex> reader_lock_t;
    typedef std::unique_lock<std::shared_mutex> writer_lock_t;

    // Store the data on executors
    std::unordered_map<std::string, std::shared_ptr<node_data>> _nodes;

    std::deque<std::weak_ptr<node_data>> _free_nodes;

    // Reader-writer lock
    std::shared_mutex _mutex;

  public:
    enum class ResultCode
    {
      OK = 0,
      EXECUTOR_EXISTS = 1,
      EXECUTOR_DOESNT_EXIST = 2,
      MALFORMED_DATA = 3
    };

    ResultCode add(const std::string& node_name, const std::string & ip_address, int port, int cores, int memory);
    ResultCode remove(const std::string& node_name);

    int lease(int numcores, int memory, rfaas::LeaseResponse & results);

    void reclaim(const std::string& node_name, int numcores, int memory);

    reader_lock_t read_lock();

    void read(const std::string &);
    void write(const std::string &);
  };

}}

#endif

