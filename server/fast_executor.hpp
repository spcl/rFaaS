
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include <vector>
#include <thread>
#include <condition_variable>

#include <rdmalib/buffer.hpp>

#include "structures.hpp"

namespace server {

  struct Server;

  struct FastExecutors {

    // Workers
    std::mutex m;
    std::condition_variable _cv;
    std::vector<std::thread> _threads;
    std::vector<ThreadStatus> _threads_status; 
    std::vector<rdmalib::Buffer<char>> _send, _rcv;
    bool _closing;
    uint32_t _numcores;
    Server & _server;

    FastExecutors(int num, int msg_size, Server &);
    ~FastExecutors();

    void enable(int thread_id, ThreadStatus && status);
    void disable(int thread_id);
    void wakeup();
    void work(int);
    // Thread implementation that uses condition variable to synchronize with server.
    void cv_thread_func(int id);
  };

}

#endif

