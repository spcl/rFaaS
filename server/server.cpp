
#include <csignal>
#include <signal.h>
#include <sys/time.h>

#include <spdlog/spdlog.h>

#include "rdmalib/buffer.hpp"
#include "rdmalib/rdmalib.hpp"
#include "rdmalib/server.hpp"
#include "server.hpp"

namespace server {

  bool SignalHandler::closing = false;

  SignalHandler::SignalHandler()
  {
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = &SignalHandler::handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    //FIXME: disable signals to avoid potential interrupts
    //sigaction(SIGINT, &sigIntHandler, nullptr);
  }

  void SignalHandler::handler(int)
  {
    SignalHandler::closing = true;
  }

  Server::Server(std::string addr, int port, int cheap_executors, int fast_executors,
      int msg_size, int rcv_buf, bool pin_threads, int max_inline_data, std::string server_file):
    _state(addr, port, rcv_buf, true, max_inline_data),
    _status(addr, port),
    _fast_exec(fast_executors, msg_size, pin_threads, *this),
    _conn(nullptr),
    _wc_buffer(rcv_buf),
    _inline_data(msg_size <= max_inline_data)
  {
    //listen();

    // TODO: optimize, single buffer + offsets
    // TODO: share between clients?
    // FIXME: valid only for the "cheap" exeuction
    //for(int i = 0; i < QUEUE_SIZE; ++i) {
    //  _queue[i] = std::move(rdmalib::Buffer<char>(QUEUE_MSG_SIZE));
    //  _queue[i].register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    //}
    // Initialize threads as currently unbusy
    //memset(_threads_allocation.data(), 0, _threads_allocation.data_size());
    //_threads_allocation.register_memory(_state.pd(), IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE |IBV_ACCESS_LOCAL_WRITE);
    //_status.set_thread_allocator(_threads_allocation);
    std::ofstream out(server_file);
    status().serialize(out);
  }

  const rdmalib::server::ServerStatus & Server::status() const
  {
    return _status;
  }

  void Server::listen()
  {
    _state.allocate();
  }

  void Server::reload_queue(rdmalib::Connection & conn, int32_t idx)
  {
    SPDLOG_DEBUG("Post receive idx: {}", idx);
    // FIXME: "cheap"
    //conn.post_recv(_queue[idx], idx);
    conn.post_recv({});
  }

  rdmalib::Connection* Server::poll_communication()
  {
    // TODO: timeout + number of retries option
    this->_conn = _state.poll_events(
      [this](rdmalib::Connection & conn) {
        this->_wc_buffer.connect(&conn);
      }
    );
    if(this->_conn)
      this->_conn->inlining(_inline_data);
    return this->_conn;
  }

  std::tuple<int, int> Server::poll_server_notify(int max_repetitions, int warmup_iters)
  {
    _fast_exec.allocate_threads(false);

    int repetitions = 0;
    int total_iters = max_repetitions + warmup_iters;
    constexpr int cores_mask = 0x3F;
    timeval start;
    _conn->notify_events();
    while(repetitions < total_iters) {

      //spdlog::info("Wait");
      auto cq = _conn->wait_events();
      _conn->notify_events();

      // if we block, we never handle the interruption
      auto wcs = _wc_buffer.poll();
      if(std::get<1>(wcs)) {

        gettimeofday(&start, nullptr);
        for(int i = 0; i < std::get<1>(wcs); ++i) {

          ibv_wc* wc = &std::get<0>(wcs)[i];
          if(wc->status) {
            spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
            continue;
          }
          int info = ntohl(wc->imm_data);
          int func_id = info >> 6;
          int core = info & cores_mask;
          // Startpoint of iteration for a given thread
          _fast_exec._start_timestamps[core] = start;
          SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);
          //uint32_t cur_invoc = server._exec.get_invocation_id();
          uint32_t cur_invoc = 0;

          // FIXME: producer-consumer interface
          SPDLOG_DEBUG("Wake-up fast thread {}", core);
          _fast_exec.enable(core,
            {
              _db.functions[func_id],
              cur_invoc,
              this->_conn
            }
          );

          // clean send queue
          this->_conn->poll_wc(rdmalib::QueueType::SEND, false);
          _wc_buffer.refill();
          ++repetitions;
        }
        _conn->ack_events(cq, std::get<1>(wcs));
      }
    }
    _fast_exec.close();

    return std::make_tuple(_fast_exec._time_sum.load(), repetitions);
  }

  std::tuple<int, int> Server::poll_server(int max_repetitions, int warmup_iters)
  {
    _fast_exec.allocate_threads(false);

    int repetitions = 0;
    int total_iters = max_repetitions + warmup_iters;
    constexpr int cores_mask = 0x3F;
    timeval start;
    while(!server::SignalHandler::closing && repetitions < total_iters) {

      // if we block, we never handle the interruption
      auto wcs = _wc_buffer.poll();
      if(std::get<1>(wcs)) {
        // FIXME: one var per thread
        gettimeofday(&start, nullptr);
        for(int i = 0; i < std::get<1>(wcs); ++i) {

          ibv_wc* wc = &std::get<0>(wcs)[i];
          if(wc->status) {
            spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
            continue;
          }
          int info = ntohl(wc->imm_data);
          int func_id = info >> 6;
          int core = info & cores_mask;
          // Startpoint of iteration for a given thread
          _fast_exec._start_timestamps[core] = start;
          SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);
          //uint32_t cur_invoc = server._exec.get_invocation_id();
          uint32_t cur_invoc = 0;

          // FIXME: producer-consumer interface
          SPDLOG_DEBUG("Wake-up fast thread {}", core);
          _fast_exec.enable(core,
            {
              _db.functions[func_id],
              cur_invoc,
              this->_conn
            }
          );

          // clean send queue
          this->_conn->poll_wc(rdmalib::QueueType::SEND, false);
          _wc_buffer.refill();
          ++repetitions;
        }
      }
    }
    _fast_exec.close();

    return std::make_tuple(_fast_exec._time_sum.load(), repetitions);
  }

  std::tuple<int, int> Server::poll_threads(int max_repetitions, int warmup_iters)
  {
    _fast_exec._conn = _conn;
    _fast_exec._wc_buffer = &_wc_buffer;
    // FIXME: parallel accumulation of reps across threads?
    // +1 to handle the warm-up call
    _fast_exec._max_repetitions = max_repetitions;
    _fast_exec._warmup_iters = warmup_iters;
    _fast_exec.allocate_threads(true);

    // FIXME: more threads
    for(int i = 0; i < _fast_exec._threads.size(); ++i)
      _fast_exec._threads[i].join();

    return std::make_tuple(_fast_exec._time_sum.load(), _fast_exec._repetitions.load());
  }

  // FIXME: shared receive queue
  //void Server::poll_srq()
  //{
  //  while(!server::SignalHandler::closing) {

  //    // if we block, we never handle the interruption
  //    std::optional<ibv_wc> wc = conn->poll_wc(rdmalib::QueueType::RECV, false);
  //    bool correct_allocation = true;
  //    if(wc) {

  //      gettimeofday(&start, nullptr);
  //      if(wc->status) {
  //        spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
  //        continue;
  //      }
  //      int info = ntohl(wc->imm_data);
  //      int func_id = info >> 6;
  //      int core = info & cores_mask;
  //      SPDLOG_DEBUG("Execute func {} at core {}", func_id, core);
  //      //uint32_t cur_invoc = server._exec.get_invocation_id();
  //      uint32_t cur_invoc = 0;
  //      server::InvocationStatus & invoc = server._exec.invocation_status(cur_invoc);
  //      invoc.connection = &*conn;
  //      server._exec.enable(core,
  //        {
  //          server._db.functions[func_id],
  //          &server._rcv[core],
  //          &server._send[core],
  //          cur_invoc
  //        }
  //      );
  //      //server._exec.wakeup();
  //      server._exec.work(core);
  //      // clean queue
  //      conn->poll_wc(rdmalib::QueueType::SEND, false);
  //      gettimeofday(&end, nullptr);
  //      int usec = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
  //      repetitions += 1;
  //      requests--;
  //      if(requests < 1) {
  //        conn->post_recv({}, -1, server._rcv_buf_size);
  //        requests = server._rcv_buf_size;
  //      }
  //      sum += usec;
  //  }
  //}


  //uint32_t Executors::get_invocation_id()
  //{
  //  if(_last_invocation == _numcores) {
  //    _last_invocation = 0;
  //    // find next empty
  //    while(_invocations_status[_last_invocation++].connection);
  //    _last_invocation--;
  //  }
  //  return _last_invocation;
  //}

  //InvocationStatus & Executors::invocation_status(int idx)
  //{
  //  return this->_invocations_status[idx];
  //}

  //void Executors::thread_func(int id)
  //{
  //  std::unique_lock<std::mutex> lk(m);
  //  rdmalib::functions::FuncType ptr = nullptr;
  //  SPDLOG_DEBUG("Thread {} created!", id);
  //  while(!_closing) {

  //    SPDLOG_DEBUG("Thread {} goes to sleep! {}", id, _closing);
  //    if(!lk.owns_lock())
  //      lk.lock();
  //    _cv.wait(lk, [this, id](){ return _threads_status[id].func || _closing; });
  //    lk.unlock();
  //    SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

  //    if(_closing) {
  //      SPDLOG_DEBUG("Thread {} exits!", id);
  //      return;
  //    }

  //    ptr = _threads_status[id].func;
  //    uint32_t invoc_id = _threads_status[id].invoc_id;
  //    //rdmalib::Connection* conn = std::get<3>(_status[id]);

  //    SPDLOG_DEBUG("Thread {} begins work! Executing function", id);
  //    // Data to ignore header passed in the buffer
  //    (*ptr)(_threads_status[id].in->data(), _threads_status[id].out->ptr());
  //    SPDLOG_DEBUG("Thread {} finished work!", id);

  //    char* data = static_cast<char*>(_threads_status[id].in->ptr());
  //    uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
  //    uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
  //    SPDLOG_DEBUG("Thread {} finished work! Write to remote addr {} rkey {}", id, r_addr, r_key);
  //    // decrease number of active instances
  //    if(--_invocations_status[invoc_id].active_threads) {
  //      // write result
  //      _invocations_status[invoc_id].connection->post_write(
  //        *_threads_status[id].out,
  //        {r_addr, r_key}
  //      );
  //    } else {
  //      uint32_t func_id = *reinterpret_cast<uint32_t*>(data + 12);
  //      // write result and signal
  //      _invocations_status[invoc_id].connection->post_write(
  //        *_threads_status[id].out,
  //        {r_addr, r_key},
  //        func_id
  //      );
  //      // TODO Clean invocation status
  //    }

  //    // clean status of thread
  //    // TODO: atomic status
  //    _threads_status[id] = {nullptr, nullptr, nullptr, 0};
  //    this->disable(id);
  //    ptr = nullptr;

  //    SPDLOG_DEBUG("Thread {} loops again!", id);
  //  } 
  //  SPDLOG_DEBUG("Thread {} exits!", id);
  //}
}
