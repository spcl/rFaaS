
#ifndef __RFAAS_RESOURCE_MANAGER_DB_HPP__
#define __RFAAS_RESOURCE_MANAGER_DB_HPP__

#include <deque>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include <rfaas/resources.hpp>

namespace rfaas { namespace resource_manager {

  struct ExecutorDB
  {
  private:
    typedef std::shared_lock<std::shared_mutex> reader_lock_t;
    typedef std::unique_lock<std::shared_mutex> writer_lock_t;

    // Store the data on executors
    //rfaas::servers _data;
    std::unordered_map<std::string, std::shared_ptr<rfaas::server_data>> _nodes;

    std::deque<std::weak_ptr<rfaas::server_data>> _free_nodes;

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

    ResultCode add(const std::string& node_name, const std::string & ip_address, int port, int cores);
    ResultCode remove(const std::string& node_name);
    reader_lock_t read_lock();

    void read(const std::string &);
    void write(const std::string &);
  };

}}

#endif

