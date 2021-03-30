
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include "rdmalib/rdmalib.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/functions.hpp>

#include "functions.hpp"

namespace rdmalib {
  struct RecvBuffer;
}

namespace server {

  // FIXME: is not movable or copyable at the moment
  struct Thread {
    Functions _functions;
    std::string addr;
    int port;
    int max_inline_data;
    int id, repetitions;
    int max_repetitions;
    uint64_t sum;
    rdmalib::Buffer<char> send, rcv;
    rdmalib::RecvBuffer wc_buffer;
    rdmalib::Connection* conn;

    Thread(std::string addr, int port, int id, int functions_size,
        int buf_size, int recv_buffer_size, int max_inline_data):
      _functions(functions_size),
      addr(addr),
      port(port),
      max_inline_data(max_inline_data),
      id(id),
      repetitions(0),
      max_repetitions(0),
      sum(0),
      send(buf_size),
      rcv(buf_size, rdmalib::functions::Submission::DATA_HEADER_SIZE),
      // +1 to handle batching of functions work completions + initial code submission
      wc_buffer(recv_buffer_size + 1),
      conn(nullptr)
    {
    }

    void work(int func_id, uint32_t in_size);
    void hot();
    void warm();
    void thread_work(int timeout);
  };

  struct FastExecutors {

    std::vector<Thread> _threads_data;
    std::vector<std::thread> _threads;
    bool _closing;
    int _numcores;
    int _max_repetitions;
    int _warmup_iters;
    int _pin_threads;

    FastExecutors(
      std::string client_addr, int port,
      int function_size,
      int numcores,
      int msg_size,
      int recv_buf_size,
      int max_inline_data,
      int pin_threads
    );
    ~FastExecutors();

    void close();
    void allocate_threads(int, int);
  };

}

#endif

