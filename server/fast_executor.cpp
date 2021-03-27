
#include <atomic>
#include <ostream>
#include <sys/time.h>
#include <sys/time.h>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>

#include <rdmalib/benchmarker.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/util.hpp>

#include "rdmalib/functions.hpp"
#include "server.hpp"
#include "fast_executor.hpp"

namespace server {

  FastExecutors::FastExecutors(int numcores, int msg_size, bool pin_threads, Server & server):
    _closing(false),
    _numcores(numcores),
    _max_repetitions(0),
    //_last_invocation(0),
    _server(server),
    _conn(nullptr),
    _pin_threads(pin_threads),
    _time_sum(0),
    _repetitions(0),
    _iterations(0)
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
    // FIXME: this should be only for 'warm'
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
    delete[] _thread_status;
  }

  void FastExecutors::allocate_threads(bool poll)
  {
    _thread_status = new std::atomic<int64_t>[_numcores];
    _cur_poller.store(_numcores - 1);
    for(int i = 0; i < _numcores; ++i) {
      _threads_status.push_back({nullptr, 0, nullptr});
      _thread_status[i].store(0);
      _threads.emplace_back(
        poll ? &FastExecutors::thread_poll_func : &FastExecutors::cv_thread_func,
        this, i
      );
      // FIXME: make sure that native handle is actually from pthreads
      if(_pin_threads) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        rdmalib::impl::expect_zero(pthread_setaffinity_np(_threads[i].native_handle(), sizeof(cpu_set_t), &cpuset));
      }
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

  void FastExecutors::work(int id, int func_id)
  {
    //auto ptr = _threads_status[id].func;
    auto ptr = _server._db.functions[func_id];
    //uint32_t invoc_id = _threads_status[id].invoc_id;
    char* data = static_cast<char*>(_rcv[id].ptr());
    uint32_t thread_id = *reinterpret_cast<uint32_t*>(data + 12);

    SPDLOG_DEBUG("Thread {} begins work! Executing function {}", id, thread_id);
    // Data to ignore header passed in the buffer
    (*ptr)(_rcv[id].data(), _send[id].ptr());
    SPDLOG_DEBUG("Thread {} finished work!", id);

    this->_thread_status[id].store(0, std::memory_order_relaxed);
    //auto * conn = _threads_status[id].connection;
    //this->disable(id);
    //rdmalib::Connection* conn = std::get<3>(_status[id]);
    uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
    uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
    _conn->post_write(
      _send[id],
      {r_addr, r_key},
      id
      //thread_id
    );
    //_invocations_status[invoc_id].connection->post_write(
    //  *conn,
    //  {r_addr, r_key},
    //  thread_id
    //);
  }

  void FastExecutors::serial_thread_poll_func(int)
  {
    uint64_t sum = 0;
    int repetitions = 0;
    int total_iters = _max_repetitions + _warmup_iters;
    constexpr int cores_mask = 0x3F;
    rdmalib::Benchmarker<2> server_processing_times{total_iters};

    // FIXME: disable signal handling
    //while(!server::SignalHandler::closing && repetitions < total_iters) {
    while(repetitions < total_iters) {

      // if we block, we never handle the interruption
      auto wcs = _wc_buffer->poll();
      if(std::get<1>(wcs)) {
        for(int i = 0; i < std::get<1>(wcs); ++i) {

          server_processing_times.start();
          ibv_wc* wc = &std::get<0>(wcs)[i];
          if(wc->status) {
            spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
            continue;
          }
          int info = ntohl(wc->imm_data);
          int func_id = info >> 6;
          int core = info & cores_mask;
          // FIXME: verify function data - valid ID
          SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);

          SPDLOG_DEBUG("Wake-up fast thread {}", core);
          work(core, func_id);

          // clean send queue
          // FIXME: this should be option - in reality, we don't want to wait for the transfer to end
          _conn->poll_wc(rdmalib::QueueType::SEND, true);
          sum += server_processing_times.end();
          repetitions += 1;
        }
        _wc_buffer->refill();
      }
    }
    server_processing_times.export_csv("server.csv", {"process", "send"});

    _time_sum.fetch_add(sum / 1000.0);
    _repetitions.fetch_add(repetitions);
  }

  void FastExecutors::thread_poll_func(int id)
  {
    uint64_t sum = 0;
    int total_iters = _max_repetitions + _warmup_iters;
    constexpr int cores_mask = 0x3F;
    rdmalib::Benchmarker<1> server_processing_times{total_iters};

    int64_t _poller_empty = -1;
    bool i_am_poller = false;
    //while(_iterations < total_iters) {
    while(true) {

      int64_t cur_poller = _cur_poller.load();
      if(cur_poller == _poller_empty) {
        i_am_poller = _cur_poller.compare_exchange_strong(
            _poller_empty, id, std::memory_order_release, std::memory_order_relaxed
        );
        SPDLOG_DEBUG("Thread {} Attempted to become a poller, result {}", id, i_am_poller);
      } else {
        i_am_poller = cur_poller == id;
      }

      // Now wait for assignment or perform polling
      if(i_am_poller) {
        SPDLOG_DEBUG("Thread {} Performs polling", id);
        while(i_am_poller && _iterations < total_iters) {
          // if we block, we never handle the interruption
          auto wcs = _wc_buffer->poll();
          if(std::get<1>(wcs)) {
            SPDLOG_DEBUG("Thread {} Polled {} wcs", id, std::get<1>(wcs));

            int work_myself = 0;
            for(int i = 0; i < std::get<1>(wcs); ++i) {

              server_processing_times.start();
              ibv_wc* wc = &std::get<0>(wcs)[i];
              if(wc->status) {
                spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
                continue;
              }
              int info = ntohl(wc->imm_data);
              int func_id = info >> 6;
              int core = info & cores_mask;
              // FIXME: verify function data - valid ID
              SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);

              if(core != id) {
                if(this->_thread_status[core].load() == 0) {
                  SPDLOG_DEBUG("Wake-up fast thread {} by thread {}", core, id);
                  // No need to release - we only pass an atomic to other thread
                  this->_thread_status[core].store(func_id);
                  _iterations += 1;
                }
                // In benchmarking, this should only happen in debug mode 
                else {
                  SPDLOG_DEBUG("Thread {} busy, send error to client", core);
                  // Send an error message "1" - thread busy
                  char* data = static_cast<char*>(_rcv[core].ptr());
                  uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
                  uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
                  _conn->post_write(
                    {},
                    {r_addr, r_key},
                    (core & cores_mask) | (1 << 6)
                  );
                }
                sum += server_processing_times.end();
              } else {
                work_myself = func_id;
              }
            }
            if(work_myself) {
              SPDLOG_DEBUG("Wake-up myself {}", id);
              _iterations += 1;
              // Release is needed to make sure that `iterations` have been updated
              _cur_poller.store(_poller_empty, std::memory_order_release);
              i_am_poller = false;
              this->_thread_status[id].store(1);
              work(id, work_myself);
              //_conn->poll_wc(rdmalib::QueueType::SEND, false);
            } else {
              SPDLOG_DEBUG("Thread {} Refill");
              _wc_buffer->refill();
              SPDLOG_DEBUG("Thread {} Refilled");
            }
          }
        }
        // we finished iterations
        if(i_am_poller) {
          // Wait for others to finish
          int64_t expected = 0;
          for(int i = 0; i < _numcores; ++i) {
            while(!_thread_status[i].compare_exchange_strong(
              expected, -1, std::memory_order_release, std::memory_order_relaxed
            ));
          }
          break;
        }
      } else {
        SPDLOG_DEBUG("Thread {} Waits for work", id);
        int64_t status = 0;
        do {
          status = this->_thread_status[id].load();
          cur_poller = _cur_poller.load();
        } while(status == 0 && cur_poller != -1);
        if(status > 0) {
          SPDLOG_DEBUG("Thread {} Got work!", id);
          work(id, status);
          // clean send queue
          // FIXME: this should be option - in reality, we don't want to wait for the transfer to end
          //_conn->poll_wc(rdmalib::QueueType::SEND, false);
          SPDLOG_DEBUG("Thread {} Finished work!", id);
        } else {
          break;
        }
      }
    }
    server_processing_times.export_csv("server.csv", {"process"});

    _time_sum.fetch_add(sum / 1000.0);
    _repetitions.fetch_add(_iterations);
    spdlog::info("Thread {} Finished!", id);
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

      work(id, 0);
      gettimeofday(&end, nullptr);
      int usec = (end.tv_sec - _start_timestamps[id].tv_sec) * 1000000 + (end.tv_usec - _start_timestamps[id].tv_usec);
      sum += usec;
      SPDLOG_DEBUG("Thread {} loops again!", id);
    } 
    SPDLOG_DEBUG("Thread {} exits!", id);
    _time_sum.fetch_add(sum);
  }
}

