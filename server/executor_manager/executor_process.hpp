
#ifndef __SERVER_EXECUTOR_MANAGER_EXECUTOR_PROCESS_HPP__
#define __SERVER_EXECUTOR_MANAGER_EXECUTOR_PROCESS_HPP__

#include "common/messages.hpp"
#include "rdmalib/rdmalib.hpp"
#include <infiniband/verbs.h>
#include <memory>
#include <chrono>

#include <rdmalib/connection.hpp>

namespace rfaas {
  struct AllocationRequest;
}

namespace executor {
  struct ManagerConnection;
}

namespace rfaas::executor_manager {

  struct ExecutorSettings;
  struct Lease;

  struct ActiveExecutor {

    enum class Status {
      RUNNING,
      FINISHED,
      FINISHED_FAIL 
    };
    typedef std::chrono::high_resolution_clock::time_point time_t;
    time_t _allocation_begin, _allocation_finished;
    rdmalib::Connection** connections;
    int connections_len;
    int cores;
    // use one single array?
    rdmalib::Buffer<common::NewClient> _messages;

    ActiveExecutor(int cores, ibv_pd * pd):
      connections(new rdmalib::Connection*[cores]),
      connections_len(0),
      cores(cores),
      _messages(cores)
    {
      _messages.register_memory(pd, IBV_ACCESS_LOCAL_WRITE);
    }

    virtual ~ActiveExecutor();
    virtual int id() const = 0;
    virtual std::tuple<Status,int> check() const = 0;
    void add_executor(rdmalib::Connection*);
  };

  struct ProcessExecutor : public ActiveExecutor
  {
    pid_t _pid;

    ProcessExecutor(int cores, time_t alloc_begin, pid_t pid, ibv_pd* pd);

    // FIXME: kill active executor
    //~ProcessExecutor();
    //void close();
    int id() const override;
    std::tuple<Status,int> check() const override;
    static ProcessExecutor* spawn(
      const rfaas::AllocationRequest & request,
      const ExecutorSettings & exec,
      const executor::ManagerConnection & conn,
      const Lease & lease,
      const rdmalib::RDMAPassive& listener
    );
  };

  struct DockerExecutor : public ActiveExecutor
  {
  };

}

#endif

