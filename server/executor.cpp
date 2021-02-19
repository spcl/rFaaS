
#include <csignal>
#include <signal.h>
#include <spdlog/spdlog.h>

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
    sigaction(SIGINT, &sigIntHandler, nullptr);

  }

  void SignalHandler::handler(int)
  {
    SignalHandler::closing = true;
  }

  Server::Server(std::string addr, int port, int numcores, int rcv_buf):
    _state(addr, port),
    _status(addr, port),
    _threads_allocation(numcores),
    _exec(numcores, *this),
    _rcv_buf_size(rcv_buf)
  {
    listen();

    // TODO: optimize, single buffer + offsets
    // TODO: share between clients?
    // FIXME: valid only for the "cheap" exeuction
    //for(int i = 0; i < QUEUE_SIZE; ++i) {
    //  _queue[i] = std::move(rdmalib::Buffer<char>(QUEUE_MSG_SIZE));
    //  _queue[i].register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    //}
    // Initialize threads as currently unbusy
    memset(_threads_allocation.data(), 0, _threads_allocation.data_size());
    _threads_allocation.register_memory(_state.pd(), IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE |IBV_ACCESS_LOCAL_WRITE);
    _status.set_thread_allocator(_threads_allocation);
  }

  void Server::allocate_send_buffers(int numcores, int size)
  {
    for(int i = 0; i < numcores; ++i) {
      _send.emplace_back(size);
      _send.back().register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    }
  }

  void Server::allocate_rcv_buffers(int numcores, int size)
  {
    for(int i = 0; i < numcores; ++i) {
      _rcv.emplace_back(size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
      _rcv.back().register_memory(_state.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
      _status.add_buffer(_rcv.back());
    }
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
    auto conn = _state.poll_events(
      [this](rdmalib::Connection & conn) {
        // FIXME: "cheap" invocation
        //for(int i = 0; i < QUEUE_SIZE; ++i)
        //  reload_queue(conn, i);
        for(int i = 0; i < _rcv_buf_size; ++i)
          reload_queue(conn, i);
      }
    );
    return conn;
  }

  Executors::Executors(int numcores, Server & server):
    _closing(false),
    _numcores(numcores),
    _last_invocation(0),
    _server(server)
  {
    _invocations_status = new InvocationStatus[numcores];
    for(int i = 0; i < numcores; ++i) {
      _threads_status.push_back({nullptr, nullptr, nullptr, 0});
      _threads.emplace_back(&Executors::fast_thread_func, this, i);

      _invocations_status[i].connection = nullptr;
      _invocations_status[i].active_threads = 0;
    }

  }

  Executors::~Executors()
  {
    spdlog::info("Executor is closing threads...");
    _closing = true;
    // wake threads, letting them exit
    wakeup();
    // TODO: this could be unsafe - wait for all invocations to finish?
    delete[] _invocations_status;
    // make sure we join before destructing
    for(auto & thread : _threads)
      thread.join();
  }

  uint32_t Executors::get_invocation_id()
  {
    if(_last_invocation == _numcores) {
      _last_invocation = 0;
      // find next empty
      while(_invocations_status[_last_invocation++].connection);
      _last_invocation--;
    }
    return _last_invocation;
  }

  void Executors::enable(int thread_id, ThreadStatus && status)
  {
    this->_threads_status[thread_id] = status;
  }

  void Executors::disable(int thread_id)
  {
    this->_threads_status[thread_id] = {nullptr, nullptr, nullptr, 0};
  }

  void Executors::wakeup()
  {
    _cv.notify_all();
  }

  InvocationStatus & Executors::invocation_status(int idx)
  {
    return this->_invocations_status[idx];
  }

  void Executors::work(int id)
  {
    auto ptr = _threads_status[id].func;
    uint32_t invoc_id = _threads_status[id].invoc_id;
    char* data = static_cast<char*>(_threads_status[id].in->ptr());
    uint32_t thread_id = *reinterpret_cast<uint32_t*>(data + 12);

    SPDLOG_DEBUG("Thread {} begins work! Executing function {}", id, thread_id);
    // Data to ignore header passed in the buffer
    (*ptr)(_threads_status[id].in->data(), _threads_status[id].out->ptr());
    SPDLOG_DEBUG("Thread {} finished work!", id);

    auto * conn = _threads_status[id].out;
    this->disable(id);
    //rdmalib::Connection* conn = std::get<3>(_status[id]);
    uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
    uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
    _invocations_status[invoc_id].connection->post_write(
      *conn,
      {r_addr, r_key},
      thread_id
    );
  }

  void Executors::fast_thread_func(int id)
  {
    std::unique_lock<std::mutex> lk(m);
    rdmalib::functions::FuncType ptr = nullptr;
    SPDLOG_DEBUG("Thread {} created!", id);
    while(!_closing) {

      SPDLOG_DEBUG("Thread {} goes to sleep! Closing {} flag {}", id, _closing, _threads_status[id].func != nullptr);
      if(!lk.owns_lock())
        lk.lock();
      _cv.wait(lk, [this, id](){ return _threads_status[id].func || _closing; });
      lk.unlock();
      SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

      if(_closing) {
        SPDLOG_DEBUG("Thread {} exits!", id);
        return;
      }

      work(id);
      //ptr = _threads_status[id].func;
      //uint32_t invoc_id = _threads_status[id].invoc_id;
      //char* data = static_cast<char*>(_threads_status[id].in->ptr());
      //uint32_t thread_id = *reinterpret_cast<uint32_t*>(data + 12);

      //SPDLOG_DEBUG("Thread {} begins work! Executing function {}", id, thread_id);
      //// Data to ignore header passed in the buffer
      //(*ptr)(_threads_status[id].in->data(), _threads_status[id].out->ptr());
      //SPDLOG_DEBUG("Thread {} finished work!", id);

      //auto * conn = _threads_status[id].out;
      //this->disable(id);
      ////rdmalib::Connection* conn = std::get<3>(_status[id]);
      //uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
      //uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
      //_invocations_status[invoc_id].connection->post_write(
      //  *conn,
      //  {r_addr, r_key},
      //  thread_id
      //);

      //_threads_status[id] = {nullptr, nullptr, nullptr, 0};
      //ptr = nullptr;

      SPDLOG_DEBUG("Thread {} loops again!", id);
    } 
    SPDLOG_DEBUG("Thread {} exits!", id);
  }

  void Executors::thread_func(int id)
  {
    std::unique_lock<std::mutex> lk(m);
    rdmalib::functions::FuncType ptr = nullptr;
    SPDLOG_DEBUG("Thread {} created!", id);
    while(!_closing) {

      SPDLOG_DEBUG("Thread {} goes to sleep! {}", id, _closing);
      if(!lk.owns_lock())
        lk.lock();
      _cv.wait(lk, [this, id](){ return _threads_status[id].func || _closing; });
      lk.unlock();
      SPDLOG_DEBUG("Thread {} wakes up! {}", id, _closing);

      if(_closing) {
        SPDLOG_DEBUG("Thread {} exits!", id);
        return;
      }

      ptr = _threads_status[id].func;
      uint32_t invoc_id = _threads_status[id].invoc_id;
      //rdmalib::Connection* conn = std::get<3>(_status[id]);

      SPDLOG_DEBUG("Thread {} begins work! Executing function", id);
      // Data to ignore header passed in the buffer
      (*ptr)(_threads_status[id].in->data(), _threads_status[id].out->ptr());
      SPDLOG_DEBUG("Thread {} finished work!", id);

      char* data = static_cast<char*>(_threads_status[id].in->ptr());
      uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
      uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
      SPDLOG_DEBUG("Thread {} finished work! Write to remote addr {} rkey {}", id, r_addr, r_key);
      // decrease number of active instances
      if(--_invocations_status[invoc_id].active_threads) {
        // write result
        _invocations_status[invoc_id].connection->post_write(
          *_threads_status[id].out,
          {r_addr, r_key}
        );
      } else {
        uint32_t func_id = *reinterpret_cast<uint32_t*>(data + 12);
        // write result and signal
        _invocations_status[invoc_id].connection->post_write(
          *_threads_status[id].out,
          {r_addr, r_key},
          func_id
        );
        // TODO Clean invocation status
      }

      // clean status of thread
      // TODO: atomic status
      _threads_status[id] = {nullptr, nullptr, nullptr, 0};
      this->disable(id);
      ptr = nullptr;

      SPDLOG_DEBUG("Thread {} loops again!", id);
    } 
    SPDLOG_DEBUG("Thread {} exits!", id);
  }
}
