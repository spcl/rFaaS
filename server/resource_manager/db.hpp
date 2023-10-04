
#ifndef __RFAAS_RESOURCE_MANAGER_DB_HPP__
#define __RFAAS_RESOURCE_MANAGER_DB_HPP__

#include <list>
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

#include "executor.hpp"

namespace rfaas { namespace resource_manager {


  struct ExecutorDB
  {
  private:

    typedef std::shared_lock<std::shared_mutex> reader_lock_t;
    typedef std::unique_lock<std::shared_mutex> writer_lock_t;

    // Store the data on executors
    //std::unordered_map<std::string, std::shared_ptr<Executor>> _nodes;
  
    Executors& _executors;

    std::unordered_map<uint32_t, Lease> _leases;
    uint32_t _lease_count;

    std::list<std::weak_ptr<Executor>> _free_nodes;

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

    ExecutorDB(Executors& executors):
      _executors(executors),
      _lease_count(0)
    {}

    ResultCode add(const std::string& node_name, const std::string & ip_address, int port, int cores, int memory);
    ResultCode remove(const std::string& node_name);

    std::shared_ptr<Executor> open_lease(int numcores, int memory, rfaas::LeaseResponse& lease);

    void close_lease(common::LeaseDeallocation & msg);

    void reclaim(const std::string& node_name, int numcores, int memory);

    reader_lock_t read_lock();

    void read(const std::string &);
    void write(const std::string &);
  };

}}

#endif

