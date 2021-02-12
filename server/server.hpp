
#include <vector>
#include <thread>
#include <condition_variable>
#include <tuple>

#include <cxxopts.hpp>

#include <rdmalib/functions.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/rdmalib.hpp>

namespace server {

  cxxopts::ParseResult opts(int argc, char ** argv);
  struct Server;

  struct SignalHandler {
    static bool closing;

    SignalHandler();

    static void handler(int);
  };

  struct Executors {

    typedef std::tuple<
      rdmalib::functions::FuncType,
      rdmalib::Buffer<char>*,
      rdmalib::Buffer<char>*,
      uint32_t
    > thread_status_t;
    typedef std::tuple<
      uint32_t,
      std::atomic<int>,
      rdmalib::Connection*
    > invoc_status_t;
    std::mutex m;
    std::vector<thread_status_t> _status; 
    invoc_status_t* _invocations;
    int _last_invocation;
    std::vector<std::thread> _threads;
    std::condition_variable _cv;
    bool _closing;
    Server & _server;

    Executors(int numcores, Server &);
    ~Executors();

    // thread-safe for different ids
    void enable(int thread_id, thread_status_t && status);
    void disable(int thread_id);
    void wakeup();
    int get_invocation_id();

    void thread_func(int id);
  };


  struct Server {
    static const int QUEUE_SIZE = 5;
    // 80 chars + 4 ints
    //static const int QUEUE_MSG_SIZE = 100;
    static const int QUEUE_MSG_SIZE = 4096;
    rdmalib::RDMAPassive _state;
    rdmalib::server::ServerStatus _status;
    std::array<rdmalib::Buffer<char>, QUEUE_SIZE> _queue;
    std::vector<rdmalib::Buffer<char>> _send, _rcv;
    rdmalib::Buffer<int> _threads_allocation;
    rdmalib::functions::FunctionsDB _db;
    Executors _exec;

    Server(std::string addr, int port, int numcores);

    void allocate_send_buffers(int numcores, int size);
    void allocate_rcv_buffers(int numcores, int size);
    void reload_queue(rdmalib::Connection & conn, int32_t idx);
    void listen();
    rdmalib::Connection* poll_communication();
    const rdmalib::server::ServerStatus & status() const;
  };

}

