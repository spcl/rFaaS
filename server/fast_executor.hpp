
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <rdmalib/buffer.hpp>

#include "rdmalib/connection.hpp"
#include "structures.hpp"

namespace rdmalib {
  struct RecvBuffer;
}

namespace server {

  struct Server;

  struct FastExecutors {

    // Workers
    std::mutex m;
    std::condition_variable _cv;
    std::vector<std::thread> _threads;
    std::vector<ThreadStatus> _threads_status; 
    std::vector<rdmalib::Buffer<char>> _send, _rcv;
    std::vector<timeval> _start_timestamps;
    bool _closing;
    int _numcores;
    int _max_repetitions;
    int _warmup_iters;
    Server & _server;
    rdmalib::Connection* _conn;
    rdmalib::RecvBuffer* _wc_buffer;

    // Statistics
    std::atomic<int> _time_sum;
    std::atomic<int> _repetitions;

    FastExecutors(int num, int msg_size, Server &);
    ~FastExecutors();

    void allocate_threads(bool poll);
    void enable(int thread_id, ThreadStatus && status);
    void disable(int thread_id);
    void wakeup();
    void close();
    void work(int);
    // Thread implementation that uses condition variable to synchronize with server.
    void cv_thread_func(int id);
    // Polling implemantation directly inside a thread
    void thread_poll_func(int);
  };

}

#endif

