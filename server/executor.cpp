
#include <spdlog/spdlog.h>

#include "rdmalib/rdmalib.hpp"
#include "rdmalib/server.hpp"
#include "server.hpp"

namespace server {

  Server::Server(std::string addr, int port, int numcores):
    _state(addr, port),
    _status(addr, port),
    _threads_allocation(numcores),
    _exec(numcores, *this)
  {
    listen();

    // TODO: optimize, single buffer + offsets
    // TODO: share between clients?
    for(int i = 0; i < QUEUE_SIZE; ++i) {
      _queue[i] = std::move(rdmalib::Buffer<char>(QUEUE_MSG_SIZE));
      _queue[i].register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    }
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
    spdlog::debug("Post receive idx: {}", idx);
    conn.post_recv(_queue[idx], idx);
  }

  rdmalib::Connection* Server::poll_communication()
  {
    // TODO: timeout + number of retries option
    auto conn = _state.poll_events(
      [this](rdmalib::Connection & conn) {
        for(int i = 0; i < QUEUE_SIZE; ++i)
          reload_queue(conn, i);
      }
    );
    return conn;
  }

  Executors::Executors(int numcores, Server & server):
    lk(m),
    _last_invocation(0),
    _server(server)
  {
    _invocations = new invoc_status_t[numcores];
    for(int i = 0; i < numcores; ++i) {
      _threads.emplace_back(&Executors::thread_func, this, i);
      _status.emplace_back(nullptr, nullptr, nullptr, 0);

      std::get<0>(_invocations[i]) = 0;
      std::get<1>(_invocations[i]) = 0;
      std::get<2>(_invocations[i]) = nullptr;
    }

  }

  int Executors::get_invocation_id()
  {
    // TODO: store numcores
    if(_last_invocation == _status.size()) {
      _last_invocation = 0;
      // find next empty
      while(std::get<0>(_invocations[_last_invocation++]));
      _last_invocation--;
    }
    return _last_invocation;
  }

  void Executors::enable(int thread_id, thread_status_t && status)
  {
    _status[thread_id] = status;
  }

  void Executors::disable(int thread_id)
  {
    _status[thread_id] = std::make_tuple(nullptr, nullptr, nullptr, 0);
  }

  void Executors::wakeup()
  {
    _cv.notify_all();
  }

  void Executors::thread_func(int id)
  {
    rdmalib::functions::FuncType ptr = nullptr;
    spdlog::debug("Thread {} created!", id);
    while(1) {

      while(!ptr) {
        _cv.wait(lk);
        ptr = std::get<0>(_status[id]);
      }
      rdmalib::Buffer<char>* args = std::get<1>(_status[id]);
      rdmalib::Buffer<char>* dest = std::get<2>(_status[id]);
      uint32_t invoc_id = std::get<3>(_status[id]);
      //rdmalib::Connection* conn = std::get<3>(_status[id]);

      spdlog::debug("Thread {} begins work! Executing function", id);
      (*ptr)(args->data(), dest->ptr());
      spdlog::debug("Thread {} finished work!", id);

      // decrease count
      char* data = static_cast<char*>(args->ptr());
      uint64_t r_addr = *reinterpret_cast<uint64_t*>(data);
      uint32_t r_key = *reinterpret_cast<uint32_t*>(data + 8);
      spdlog::debug("Thread {} finished work! Write to remote addr {} rkey {}", id, r_addr, r_key);
      if(--std::get<1>(_invocations[invoc_id])) {
        // write result
        std::get<2>(_invocations[invoc_id])->post_write(
          *dest,
          {r_addr, r_key}
        );
      } else {
        uint32_t func_id = *reinterpret_cast<uint32_t*>(data + 12);
        // write result and signal
        std::get<2>(_invocations[invoc_id])->post_write(
          *dest,
          {r_addr, r_key},
          func_id
        );
        // TODO Clean invocation status
      }
      // clean status of thread
      // TODO: atomic status
      _status[id] = std::make_tuple(nullptr, nullptr, nullptr, 0);

      this->disable(id);
      ptr = nullptr;
    } 

  }
}
