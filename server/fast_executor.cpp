
#include <ostream>
#include <spdlog/spdlog.h>

#include "server.hpp"
#include "fast_executor.hpp"
#include "spdlog/common.h"

namespace server {

  FastExecutors::FastExecutors(int numcores, int msg_size, Server & server):
    _closing(false),
    _numcores(numcores),
    //_last_invocation(0),
    _server(server)
  {
    for(int i = 0; i < numcores; ++i) {
      _threads_status.push_back({nullptr, 0, nullptr});
      _threads.emplace_back(&FastExecutors::cv_thread_func, this, i);
    }

    for(int i = 0; i < numcores; ++i) {
      _send.emplace_back(msg_size);
      _server.register_buffer(_send.back(), false);
    }

    for(int i = 0; i < numcores; ++i) {
      _rcv.emplace_back(msg_size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
      _server.register_buffer(_rcv.back(), true);
    }
    //_invocations_status = new InvocationStatus[numcores];
    //  _invocations_status[i].connection = nullptr;
    //  _invocations_status[i].active_threads = 0;
    //}
  }

  FastExecutors::~FastExecutors()
  {
    spdlog::info("FastExecutor is closing threads...");
    _closing = true;
    // wake threads, letting them exit
    wakeup();
    // TODO: this could be unsafe - wait for all invocations to finish?
    //delete[] _invocations_status;
    // make sure we join before destructing
    for(auto & thread : _threads)
      thread.join();
  }

  void FastExecutors::enable(int thread_id, ThreadStatus && status)
  {
    std::lock_guard<std::mutex> g(m);
    this->_threads_status[thread_id] = status;
    _cv.notify_all();
  }

  void FastExecutors::disable(int thread_id)
  {
    this->_threads_status[thread_id] = {nullptr, 0, nullptr};
  }

  void FastExecutors::wakeup()
  {
    _cv.notify_all();
  }

  void FastExecutors::work(int id)
  {
    auto ptr = _threads_status[id].func;
    //uint32_t invoc_id = _threads_status[id].invoc_id;
    char* data = static_cast<char*>(_rcv[id].ptr());
    uint32_t thread_id = *reinterpret_cast<uint32_t*>(data + 12);

    SPDLOG_DEBUG("Thread {} begins work! Executing function {}", id, thread_id);
    // Data to ignore header passed in the buffer
    (*ptr)(_rcv[id].data(), _send[id].ptr());
    SPDLOG_DEBUG("Thread {} finished work!", id);

    auto * conn = _threads_status[id].connection;
    this->disable(id);
    //rdmalib::Connection* conn = std::get<3>(_status[id]);
    uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
    uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
    conn->post_write(
      _send[id],
      {r_addr, r_key},
      thread_id
    );
    //_invocations_status[invoc_id].connection->post_write(
    //  *conn,
    //  {r_addr, r_key},
    //  thread_id
    //);
  }

  void FastExecutors::cv_thread_func(int id)
  {
    std::unique_lock<std::mutex> lk(m);
    SPDLOG_DEBUG("Thread {} created!", id);
    while(!_closing) {

      SPDLOG_DEBUG("Thread {} goes to sleep! Closing {} ptr {}", id, _closing, _threads_status[id].func != nullptr);
      if(!lk.owns_lock())
       lk.lock();
      _cv.wait(lk, [this, id](){
          return _threads_status[id].func || _closing;
      });
      lk.unlock();
      SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

      if(_closing) {
        SPDLOG_DEBUG("Thread {} exits!", id);
        return;
      }

      work(id);
      SPDLOG_DEBUG("Thread {} loops again!", id);
    } 
    SPDLOG_DEBUG("Thread {} exits!", id);
  }
}

