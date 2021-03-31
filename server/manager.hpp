
#ifndef __SERVER_EXECUTOR_MANAGER__
#define __SERVER_EXECUTOR_MANAGER__

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

namespace rdmalib {
  struct AllocationRequest;
}

namespace executor {

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

    virtual ~ActiveExecutor();
    virtual int id() const = 0;
    virtual std::tuple<Status,int> check() const = 0;
  };

  struct ProcessExecutor : public ActiveExecutor
  {
    pid_t _pid;

    ProcessExecutor(pid_t pid);

    // FIXME: kill active executor
    //~ProcessExecutor();
    //void close();
    int id() const override;
    std::tuple<Status,int> check() const override;
    static ProcessExecutor* spawn(const rdmalib::AllocationRequest & request, const ExecutorSettings & exec);
  };

  struct DockerExecutor : public ActiveExecutor
  {
  };

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection* connection;
    rdmalib::Buffer<rdmalib::AllocationRequest> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    std::unique_ptr<ActiveExecutor> executor;

    Client(rdmalib::Connection* conn, ibv_pd* pd);
    void reload_queue();
    void reinitialize(rdmalib::Connection* conn);
    void disable();
    bool active();
  };

  struct Manager
  {
    static constexpr int MAX_CLIENTS_ACTIVE = 128;
    std::mutex clients;
    std::vector<Client> _clients;
    std::atomic<int> _clients_active;
    rdmalib::RDMAPassive _state;
    rdmalib::server::ServerStatus _status;
    ExecutorSettings _settings;

    Manager(std::string addr, int port, std::string server_file, const ExecutorSettings & settings);

    void start();
    void listen();
    void poll_rdma();
  };

}

#endif

