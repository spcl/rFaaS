
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

  struct Executors {

    std::mutex m;
    std::unique_lock<std::mutex> lk;
    std::vector<std::tuple<rdmalib::functions::FuncType, void*>> _status; 
    std::vector<std::thread> _threads;
    std::condition_variable _cv;

    Executors(int numcores);

    // thread-safe for different ids
    void enable(int thread_id, rdmalib::functions::FuncType func, void* args);
    void disable(int thread_id);
    void wakeup();

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
    rdmalib::functions::FunctionsDB _db;
    Executors _exec;

    Server(std::string addr, int port, int numcores);

    void allocate_send_buffers(int numcores, int size);
    void allocate_rcv_buffers(int numcores, int size);
    void reload_queue(rdmalib::Connection & conn, int32_t idx);
    void listen();
    std::optional<rdmalib::Connection> poll_communication();
    const rdmalib::server::ServerStatus & status() const;
  };

}

