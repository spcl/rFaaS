
#include <atomic>
#include <ostream>
#include <sys/time.h>
#include <sys/time.h>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>

#include <rdmalib/allocation.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/util.hpp>
#include "rdmalib/buffer.hpp"
#include "rdmalib/connection.hpp"
#include "rdmalib/functions.hpp"
#include "rdmalib/rdmalib.hpp"

#include "server.hpp"
#include "fast_executor.hpp"

namespace server {

  void Thread::work(int invoc_id, int func_id, uint32_t in_size)
  {
    // FIXME: load func ptr
    rdmalib::functions::Submission* header = reinterpret_cast<rdmalib::functions::Submission*>(rcv.ptr());
    auto ptr = _functions.function(func_id);

    SPDLOG_DEBUG("Thread {} begins work! Executing function {} with size {}", id, _functions._names[func_id], in_size);
    // Data to ignore header passed in the buffer
    (*ptr)(rcv.data(), in_size, send.ptr());
    SPDLOG_DEBUG("Thread {} finished work!", id);

    // Send back: the value of immediate write
    // first 16 bytes - invocation id
    // second 16 bytes - return value (0 on no error)
    conn->post_write(
      send,
      {header->r_address, header->r_key},
      (invoc_id << 16) | 0,
      send.bytes() <= max_inline_data
    );
  }

  void Thread::hot()
  {
    rdmalib::Benchmarker<1> server_processing_times{max_repetitions};
    SPDLOG_DEBUG("Thread {} Begins hot polling", id);

    while(repetitions < max_repetitions) {

      // if we block, we never handle the interruption
      auto wcs = wc_buffer.poll();
      if(std::get<1>(wcs)) {
        for(int i = 0; i < std::get<1>(wcs); ++i) {

          server_processing_times.start();
          ibv_wc* wc = &std::get<0>(wcs)[i];
          if(wc->status) {
            spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
            continue;
          }
          int info = ntohl(wc->imm_data);
          int func_id = info & invocation_mask;
          int invoc_id = info >> 16;
          SPDLOG_DEBUG(
            "Thread {} Invoc id {} Execute func {} Repetition {}",
            id, invoc_id, func_id, repetitions
          );

          work(invoc_id, func_id, wc->byte_len);

          sum += server_processing_times.end();
          conn->poll_wc(rdmalib::QueueType::SEND, true);
          repetitions += 1;
        }
        wc_buffer.refill();
      }
    }
  }

  void Thread::warm()
  {
    rdmalib::Benchmarker<1> server_processing_times{max_repetitions};
    // FIXME: this should be automatic
    SPDLOG_DEBUG("Thread {} Begins warm polling", id);

    while(repetitions < max_repetitions) {

      auto cq = conn->wait_events();
      conn->ack_events(cq, 1);
      conn->notify_events();

      // if we block, we never handle the interruption
      auto wcs = wc_buffer.poll();
      if(std::get<1>(wcs)) {
        for(int i = 0; i < std::get<1>(wcs); ++i) {

          server_processing_times.start();
          ibv_wc* wc = &std::get<0>(wcs)[i];
          if(wc->status) {
            spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
            continue;
          }
          int info = ntohl(wc->imm_data);
          int func_id = info & invocation_mask;
          int invoc_id = info >> 16;
          SPDLOG_DEBUG(
            "Thread {} Invoc id {} Execute func {} Repetition {}",
            id, invoc_id, func_id, repetitions
          );

          work(invoc_id, func_id, wc->byte_len);

          sum += server_processing_times.end();
          conn->poll_wc(rdmalib::QueueType::SEND, true);
          repetitions += 1;
        }
        wc_buffer.refill();
      }
    }
  }

  void Thread::thread_work(int timeout)
  {
    // FIXME: why rdmaactive needs rcv_buf_size?
    rdmalib::RDMAActive active(addr, port, wc_buffer._rcv_buf_size, max_inline_data);
    rdmalib::Buffer<char> func_buffer(_functions.memory(), _functions.size());

    active.allocate();
    this->conn = &active.connection();
    this->conn->initialize();
    // Receive function data from the client - this WC must be posted first
    // We do it before connection to ensure that client does not start sending before us
    func_buffer.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    this->conn->post_recv(func_buffer);

    // Request notification before connecting - avoid missing a WC!
    // Do it only when starting from a warm directly
    if(timeout == 0)
      conn->notify_events();

    if(!active.connect())
      return;

    // Now generic receives for function invocations
    send.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
    rcv.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    this->wc_buffer.connect(this->conn);
    spdlog::info("Thread {} Established connection to client!", id);

    // Send to the client information about thread buffer
    rdmalib::Buffer<rdmalib::BufferInformation> buf(1);
    buf.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
    buf.data()[0].r_addr = rcv.address();
    buf.data()[0].r_key = rcv.rkey();
    SPDLOG_DEBUG("Thread {} Sends buffer details to client!", id);
    this->conn->post_send(buf, 0, buf.size() <= max_inline_data);
    this->conn->poll_wc(rdmalib::QueueType::SEND, true, 1);
    SPDLOG_DEBUG("Thread {} Sent buffer details to client!", id);

    // We should have received functions data - just one message
    auto wcs = this->conn->poll_wc(rdmalib::QueueType::RECV, true, 1);
    _functions.process_library();
    SPDLOG_DEBUG("Thread {} Received functions library of size {}", id, std::get<0>(wcs)[0].byte_len);

    spdlog::info("Thread {} begins work with timeout {}", id, timeout);
    if(timeout == -1) {
      hot();
    } else if(timeout == 0) {
      warm();
    } else {
      //FIXME: here implement switching
    }
    spdlog::info("Thread {} finished work", id);
  }

  FastExecutors::FastExecutors(std::string client_addr, int port,
      int func_size,
      int numcores,
      int msg_size,
      int recv_buf_size,
      int max_inline_data,
      int pin_threads
  ):
    _closing(false),
    _numcores(numcores),
    _max_repetitions(0),
    _pin_threads(pin_threads)
  {
    // Reserve place to ensure that no reallocations happen
    _threads_data.reserve(numcores);
    for(int i = 0; i < numcores; ++i)
      _threads_data.emplace_back(client_addr, port, i, func_size, msg_size, recv_buf_size, max_inline_data);
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
    //{
    //  std::lock_guard<std::mutex> g(m);
    //  _closing = true;
    //  // wake threads, letting them exit
    //  wakeup();
    //}
    // make sure we join before destructing
    for(auto & thread : _threads)
      // Might have been closed earlier
      if(thread.joinable())
        thread.join();

    for(auto & thread : _threads_data)
      spdlog::info("Thread {} Repetitions {} Avg time {}",
          thread.id,
          thread.repetitions,
          static_cast<double>(thread.sum) / thread.repetitions / 1000.0
      );
    _closing = true;
  }

  void FastExecutors::allocate_threads(int timeout, int iterations)
  {
    int pin_threads = _pin_threads;
    for(int i = 0; i < _numcores; ++i) {
      _threads_data[i].max_repetitions = iterations;
      _threads.emplace_back(
        &Thread::thread_work,
        &_threads_data[i],
        timeout
      );
      // FIXME: make sure that native handle is actually from pthreads
      if(pin_threads) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(pin_threads++, &cpuset);
        rdmalib::impl::expect_zero(pthread_setaffinity_np(
          _threads[i].native_handle(),
          sizeof(cpu_set_t), &cpuset
        ));
      }
    }
  }

  //void FastExecutors::serial_thread_poll_func(int)
  //{
  //  uint64_t sum = 0;
  //  int repetitions = 0;
  //  int total_iters = _max_repetitions + _warmup_iters;
  //  constexpr int cores_mask = 0x3F;
  //  rdmalib::Benchmarker<2> server_processing_times{total_iters};

  //  // FIXME: disable signal handling
  //  //while(!server::SignalHandler::closing && repetitions < total_iters) {
  //  while(repetitions < total_iters) {

  //    // if we block, we never handle the interruption
  //    auto wcs = _wc_buffer->poll();
  //    if(std::get<1>(wcs)) {
  //      for(int i = 0; i < std::get<1>(wcs); ++i) {

  //        server_processing_times.start();
  //        ibv_wc* wc = &std::get<0>(wcs)[i];
  //        if(wc->status) {
  //          spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
  //          continue;
  //        }
  //        int info = ntohl(wc->imm_data);
  //        int func_id = info >> 6;
  //        int core = info & cores_mask;
  //        // FIXME: verify function data - valid ID
  //        SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);

  //        SPDLOG_DEBUG("Wake-up fast thread {}", core);
  //        work(core, func_id);

  //        // clean send queue
  //        // FIXME: this should be option - in reality, we don't want to wait for the transfer to end
  //        sum += server_processing_times.end();
  //        _conn->poll_wc(rdmalib::QueueType::SEND, true);
  //        repetitions += 1;
  //      }
  //      _wc_buffer->refill();
  //    }
  //  }
  //  server_processing_times.export_csv("server.csv", {"process", "send"});

  //  _time_sum.fetch_add(sum / 1000.0);
  //  _repetitions.fetch_add(repetitions);
  //}

  //void FastExecutors::thread_poll_func(int id)
  //{
  //  uint64_t sum = 0;
  //  int total_iters = _max_repetitions + _warmup_iters;
  //  constexpr int cores_mask = 0x3F;
  //  rdmalib::Benchmarker<1> server_processing_times{total_iters};

  //  int64_t _poller_empty = -1;
  //  bool i_am_poller = false;
  //  //while(_iterations < total_iters) {
  //  while(true) {

  //    int64_t cur_poller = _cur_poller.load();
  //    if(cur_poller == _poller_empty) {
  //      i_am_poller = _cur_poller.compare_exchange_strong(
  //          _poller_empty, id, std::memory_order_release, std::memory_order_relaxed
  //      );
  //      SPDLOG_DEBUG("Thread {} Attempted to become a poller, result {}", id, i_am_poller);
  //    } else {
  //      i_am_poller = cur_poller == id;
  //    }

  //    // Now wait for assignment or perform polling
  //    if(i_am_poller) {
  //      SPDLOG_DEBUG("Thread {} Performs polling", id);
  //      while(i_am_poller && _iterations < total_iters) {
  //        // if we block, we never handle the interruption
  //        auto wcs = _wc_buffer->poll();
  //        if(std::get<1>(wcs)) {
  //          SPDLOG_DEBUG("Thread {} Polled {} wcs", id, std::get<1>(wcs));

  //          int work_myself = 0;
  //          for(int i = 0; i < std::get<1>(wcs); ++i) {

  //            server_processing_times.start();
  //            ibv_wc* wc = &std::get<0>(wcs)[i];
  //            if(wc->status) {
  //              spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
  //              continue;
  //            }
  //            int info = ntohl(wc->imm_data);
  //            int func_id = info >> 6;
  //            int core = info & cores_mask;
  //            // FIXME: verify function data - valid ID
  //            SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);

  //            if(core != id) {
  //              if(this->_thread_status[core].load() == 0) {
  //                SPDLOG_DEBUG("Wake-up fast thread {} by thread {}", core, id);
  //                // No need to release - we only pass an atomic to other thread
  //                this->_thread_status[core].store(func_id);
  //                _iterations += 1;
  //              }
  //              // In benchmarking, this should only happen in debug mode 
  //              else {
  //                SPDLOG_DEBUG("Thread {} busy, send error to client", core);
  //                // Send an error message "1" - thread busy
  //                char* data = static_cast<char*>(_rcv[core].ptr());
  //                uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
  //                uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
  //                _conn->post_write(
  //                  {},
  //                  {r_addr, r_key},
  //                  (core & cores_mask) | (1 << 6)
  //                );
  //              }
  //              sum += server_processing_times.end();
  //            } else {
  //              work_myself = func_id;
  //            }
  //          }
  //          if(work_myself) {
  //            SPDLOG_DEBUG("Wake-up myself {}", id);
  //            _iterations += 1;
  //            // Release is needed to make sure that `iterations` have been updated
  //            _cur_poller.store(_poller_empty, std::memory_order_release);
  //            i_am_poller = false;
  //            this->_thread_status[id].store(1);
  //            work(id, work_myself);
  //            //_conn->poll_wc(rdmalib::QueueType::SEND, false);
  //          }
  //          SPDLOG_DEBUG("Thread {} Refill");
  //          _wc_buffer->refill();
  //          _conn->poll_wc(rdmalib::QueueType::SEND, false);
  //          SPDLOG_DEBUG("Thread {} Refilled");
  //        }
  //      }
  //      // we finished iterations
  //      if(i_am_poller) {
  //        // Wait for others to finish
  //        int64_t expected = 0;
  //        for(int i = 0; i < _numcores; ++i) {
  //          while(!_thread_status[i].compare_exchange_strong(
  //            expected, -1, std::memory_order_release, std::memory_order_relaxed
  //          ));
  //        }
  //        break;
  //      }
  //    } else {
  //      SPDLOG_DEBUG("Thread {} Waits for work", id);
  //      int64_t status = 0;
  //      do {
  //        status = this->_thread_status[id].load();
  //        cur_poller = _cur_poller.load();
  //      } while(status == 0 && cur_poller != -1);
  //      if(status > 0) {
  //        SPDLOG_DEBUG("Thread {} Got work!", id);
  //        work(id, status);
  //        // clean send queue
  //        // FIXME: this should be option - in reality, we don't want to wait for the transfer to end
  //        //_conn->poll_wc(rdmalib::QueueType::SEND, false);
  //        SPDLOG_DEBUG("Thread {} Finished work!", id);
  //      } else {
  //        break;
  //      }
  //    }
  //  }
  //  server_processing_times.export_csv("server.csv", {"process"});

  //  _time_sum.fetch_add(sum / 1000.0);
  //  _repetitions.fetch_add(_iterations);
  //  spdlog::info("Thread {} Finished!", id);
  //}

  //void FastExecutors::cv_thread_func(int id)
  //{
  //  int sum = 0;
  //  timeval end;
  //  std::unique_lock<std::mutex> lk(m);
  //  SPDLOG_DEBUG("Thread {} created!", id);
  //  while(true) {

  //    SPDLOG_DEBUG("Thread {} goes to sleep! Closing {} ptr {}", id, _closing, _threads_status[id].func != nullptr);
  //    if(!lk.owns_lock())
  //     lk.lock();
  //    _cv.wait(lk, [this, id](){
  //        return _threads_status[id].func || _closing;
  //    });
  //    lk.unlock();
  //    SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

  //    // We don't exit unless there's no more work to do.
  //    if(_closing && !_threads_status[id].func) {
  //      SPDLOG_DEBUG("Thread {} exits!", id);
  //      break;
  //    }

  //    work(id, 0);
  //    gettimeofday(&end, nullptr);
  //    int usec = (end.tv_sec - _start_timestamps[id].tv_sec) * 1000000 + (end.tv_usec - _start_timestamps[id].tv_usec);
  //    sum += usec;
  //    SPDLOG_DEBUG("Thread {} loops again!", id);
  //  } 
  //  SPDLOG_DEBUG("Thread {} exits!", id);
  //  _time_sum.fetch_add(sum);
  //}
}

