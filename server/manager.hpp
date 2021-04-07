
#ifndef __SERVER_EXECUTOR_MANAGER__
#define __SERVER_EXECUTOR_MANAGER__

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

#include "common.hpp"
#include "readerwriterqueue.h"

namespace rdmalib {
  struct AllocationRequest;
}

namespace executor {

  // FIXME: Memory
  struct Accounting {
    volatile uint64_t hot_polling_time;
    volatile uint64_t execution_time; 
  };

  struct ExecutorSettings
  {
    bool use_docker;
    int repetitions;
    int warmup_iters;
    int recv_buffer_size;
    int max_inline_data;
  };

  struct Options {
    std::string address;
    int port;
    bool pin_threads;
    std::string server_file;
    bool verbose;
    // Passed to the scheduled executor
    ExecutorSettings exec;
  };
  Options opts(int, char**);

  struct ActiveExecutor {

    enum class Status {
      RUNNING,
      FINISHED,
      FINISHED_FAIL 
    };
    typedef std::chrono::high_resolution_clock::time_point time_t;
    time_t _allocation_begin, _allocation_finished;
    std::unique_ptr<rdmalib::Connection>* connections;
    int connections_len;
    int cores;

    ActiveExecutor(int cores):
      connections(new std::unique_ptr<rdmalib::Connection>[cores]),
      connections_len(0),
      cores(cores)
    {}

    virtual ~ActiveExecutor();
    virtual int id() const = 0;
    virtual std::tuple<Status,int> check() const = 0;
  };

  struct ProcessExecutor : public ActiveExecutor
  {
    pid_t _pid;

    ProcessExecutor(int cores, time_t alloc_begin, pid_t pid);

    // FIXME: kill active executor
    //~ProcessExecutor();
    //void close();
    int id() const override;
    std::tuple<Status,int> check() const override;
    static ProcessExecutor* spawn(
      const rdmalib::AllocationRequest & request,
      const ExecutorSettings & exec,
      const ManagerConnection & conn
    );
  };

  struct DockerExecutor : public ActiveExecutor
  {
  };

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    std::unique_ptr<rdmalib::Connection> connection;
    rdmalib::Buffer<rdmalib::AllocationRequest> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    std::unique_ptr<ActiveExecutor> executor;
    //Accounting & accounting;
    rdmalib::Buffer<Accounting> accounting;
    uint32_t allocation_time;
    bool _active;

    //Client(std::unique_ptr<rdmalib::Connection> conn, ibv_pd* pd, Accounting & _acc);
    Client(std::unique_ptr<rdmalib::Connection> conn, ibv_pd* pd);
    void reload_queue();
    //void reinitialize(rdmalib::Connection* conn);
    //void disable(int, Accounting & acc);
    void disable(int);
    bool active();
  };

  struct Manager
  {
    // FIXME: we need a proper data structure that is thread-safe and scales
    //static constexpr int MAX_CLIENTS_ACTIVE = 128;
    static constexpr int MAX_EXECUTORS_ACTIVE = 8;
    static constexpr int MAX_CLIENTS_ACTIVE = 1024;
    moodycamel::ReaderWriterQueue<std::pair<int, std::unique_ptr<rdmalib::Connection>>> _q1;
    moodycamel::ReaderWriterQueue<std::pair<int,Client>> _q2;

    std::mutex clients;
    std::map<int, Client> _clients;
    int _ids;

    //std::vector<Client> _clients;
    //std::atomic<int> _clients_active;
    rdmalib::RDMAPassive _state;
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

