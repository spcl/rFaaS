
#include <vector>
#include <thread>
#include <condition_variable>
#include <tuple>

#include <rdmalib/functions.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/connection.hpp>

#include "fast_executor.hpp"

namespace server {

  struct Server;

  struct SignalHandler {
    static bool closing;

    SignalHandler();

    static void handler(int);
  };

  struct Options {

    enum class PollingMgr {
      SERVER=0,
      SERVER_NOTIFY,
      THREAD
    };

    enum class PollingType {
      WC=0,
      DRAM
    };

    std::string address;
    int port;
    int cheap_executors, fast_executors;
    int recv_buffer_size;
    int msg_size;
    int repetitions;
    int warmup_iters;
    int pin_threads;
    int max_inline_data;
    int func_size;
    int timeout;
    bool verbose;
    PollingMgr polling_manager;
    PollingType polling_type;

    std::string mgr_address;
    int mgr_port;
    int mgr_secret;
    uint64_t accounting_buffer_addr;
    uint32_t accounting_buffer_rkey;
  };

  Options opts(int argc, char ** argv);

  //struct InvocationStatus {
  //  rdmalib::Connection* connection;
  //  std::atomic<int> active_threads;
  //};

  //struct Executors {

  //  // Workers
  //  std::mutex m;
  //  std::vector<std::thread> _threads;
  //  std::condition_variable _cv;
  //  std::vector<ThreadStatus> _threads_status; 
  //  bool _closing;
  //  uint32_t _numcores;
  //  rdmalib::Buffer<int> _threads_allocation;

  //  // Invocations
  //  InvocationStatus* _invocations_status; 
  //  uint32_t _last_invocation;
  //  Server & _server;

  //  Executors(int numcores, Server &);
  //  ~Executors();

  //  // thread-safe for different ids
  //  void enable(int thread_id, ThreadStatus && status);
  //  void disable(int thread_id);
  //  void wakeup();
  //  uint32_t get_invocation_id();
  //  InvocationStatus & invocation_status(int idx);

  //  void work(int);
  //  void thread_func(int id);
  //  void fast_thread_func(int id);
  //};



  struct Server {

    // FIXME: "cheap" invocation
    //static const int QUEUE_SIZE = 500;
    // 80 chars + 4 ints
    //static const int QUEUE_MSG_SIZE = 100;
    //static const int QUEUE_MSG_SIZE = 4096;
    //std::array<rdmalib::Buffer<char>, QUEUE_SIZE> _queue;
    rdmalib::RDMAPassive _state;
    rdmalib::server::ServerStatus _status;
    rdmalib::functions::FunctionsDB _db;
    //Executors _exec;
    FastExecutors _fast_exec;
    rdmalib::Connection* _conn;
    bool _inline_data;

    Server(
        std::string addr,
        int port,
        int cheap_executors,
        int fast_executors,
        int msg_size,
        int rcv_buf,
        bool pin_threads,
        int max_inline_data,
        std::string server_file
    );

    template<typename T>
    void register_buffer(rdmalib::Buffer<T> & buf, bool is_recv_buffer)
    {
      if(is_recv_buffer) {
        buf.register_memory(_state.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
        _status.add_buffer(buf);
      } else {
        buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
      }
    }

    //void allocate_send_buffers(int numcores, int size);
    //void allocate_rcv_buffers(int numcores, int size);
    void reload_queue(rdmalib::Connection & conn, int32_t idx);
    void listen();
    rdmalib::RDMAPassive & state();
    rdmalib::Connection* poll_communication();
    const rdmalib::server::ServerStatus & status() const;

    std::tuple<int, int> poll_server(int, int);
    std::tuple<int, int> poll_threads(int, int);
    std::tuple<int, int> poll_server_notify(int, int);

    // FIXME: shared receive queue
    //void poll_srq();
  };

}

