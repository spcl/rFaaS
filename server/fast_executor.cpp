
#include <ostream>
#include <sys/time.h>
#include <sys/time.h>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>

#include <rdmalib/recv_buffer.hpp>

#include "server.hpp"
#include "fast_executor.hpp"

namespace server {

  FastExecutors::FastExecutors(int numcores, int msg_size, Server & server):
    _closing(false),
    _numcores(numcores),
    _max_repetitions(0),
    //_last_invocation(0),
    _server(server),
    _conn(nullptr),
    _time_sum(0),
    _repetitions(0)
  {
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
    close();
  }

  void FastExecutors::close()
  {
    if(_closing)
      return;
    {
      std::lock_guard<std::mutex> g(m);
      _closing = true;
      // wake threads, letting them exit
      wakeup();
    }
    // make sure we join before destructing
    for(auto & thread : _threads)
      // Might have been closed earlier
      if(thread.joinable())
        thread.join();
  }

  void FastExecutors::allocate_threads(bool poll)
  {
    for(int i = 0; i < _numcores; ++i) {
      _threads_status.push_back({nullptr, 0, nullptr});
      _threads.emplace_back(
        poll ? &FastExecutors::thread_poll_func : &FastExecutors::cv_thread_func,
        this, i
      );
      _start_timestamps.emplace_back();
    }
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

  void FastExecutors::thread_poll_func(int)
  {
    timeval start, end;
    int sum = 0;
    int repetitions = 0;
    int total_iters = _max_repetitions + _warmup_iters;
    constexpr int cores_mask = 0x3F;
    //timeval start, end;
    while(!server::SignalHandler::closing && repetitions < total_iters) {

      // if we block, we never handle the interruption
      auto wc = _wc_buffer->poll();
      if(wc) {
        gettimeofday(&start, nullptr);
        if(wc->status) {
          spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
          continue;
        }
        int info = ntohl(wc->imm_data);
        int func_id = info >> 6;
        int core = info & cores_mask;
        SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);
        //uint32_t cur_invoc = server._exec.get_invocation_id();
        uint32_t cur_invoc = 0;

        SPDLOG_DEBUG("Wake-up fast thread {}", core);
        this->_threads_status[core] = std::move(
          ThreadStatus{
            _server._db.functions[func_id],
            cur_invoc,
            _conn
          }
        );
        work(core);

        // clean send queue
        _conn->poll_wc(rdmalib::QueueType::SEND, false);
        gettimeofday(&end, nullptr);
        int usec = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        repetitions += 1;
        sum += usec;
        _wc_buffer->refill();
      }
    }
    _time_sum.fetch_add(sum);
    _repetitions.fetch_add(repetitions);
  }

  void FastExecutors::cv_thread_func(int id)
  {
    int sum = 0;
    timeval end;
    std::unique_lock<std::mutex> lk(m);
    SPDLOG_DEBUG("Thread {} created!", id);
    while(true) {

      SPDLOG_DEBUG("Thread {} goes to sleep! Closing {} ptr {}", id, _closing, _threads_status[id].func != nullptr);
      if(!lk.owns_lock())
       lk.lock();
      _cv.wait(lk, [this, id](){
          return _threads_status[id].func || _closing;
      });
      lk.unlock();
      SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

      // We don't exit unless there's no more work to do.
      if(_closing && !_threads_status[id].func) {
        SPDLOG_DEBUG("Thread {} exits!", id);
        break;
      }

      work(id);
      gettimeofday(&end, nullptr);
      int usec = (end.tv_sec - _start_timestamps[id].tv_sec) * 1000000 + (end.tv_usec - _start_timestamps[id].tv_usec);
      sum += usec;
      SPDLOG_DEBUG("Thread {} loops again!", id);
    } 
    SPDLOG_DEBUG("Thread {} exits!", id);
    _time_sum.fetch_add(sum);
  }
}

