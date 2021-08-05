
#ifndef __RFAAS_RESOURCE_MANAGER_DB_HPP__
#define __RFAAS_RESOURCE_MANAGER_DB_HPP__

#include <iostream>
#include <shared_mutex>
#include <thread>

#include <rfaas/resources.hpp>

namespace rfaas { namespace resource_manager {

  struct ExecutorDB
  {
  private:
    typedef std::shared_lock<std::shared_mutex> lock_t;
    // Store the data on executors
    rfaas::servers _data;
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


    ResultCode add(const std::string & request);
    ResultCode remove(const std::string & request);
    lock_t read_lock();

    void read(const std::string &);
    void write(const std::string &);
  };

}}

#endif

