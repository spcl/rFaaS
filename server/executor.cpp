
#include <spdlog/spdlog.h>

#include "rdmalib/rdmalib.hpp"
#include "rdmalib/server.hpp"
#include "server.hpp"

namespace server {

  Server::Server(std::string addr, int port, int numcores):
    _state(addr, port),
    _status(addr, port),
    _exec(numcores)
  {
    listen();

    // TODO: optimize, single buffer + offsets
    // TODO: share between clients?
    for(int i = 0; i < QUEUE_SIZE; ++i) {
      _queue[i] = std::move(rdmalib::Buffer<char>(QUEUE_MSG_SIZE));
      _queue[i].register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    }
  
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
      _rcv.emplace_back(size);
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
    _state.post_recv(conn, _queue[idx], idx);
  }

  std::optional<rdmalib::Connection> Server::poll_communication()
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

  Executors::Executors(int numcores):
    lk(m)
  {
    for(int i = 0; i < numcores; ++i) {
      _threads.emplace_back(&Executors::thread_func, this, i);
      _status.emplace_back(nullptr, nullptr);
    }

  }

  void Executors::enable(int thread_id, rdmalib::functions::FuncType func, void* args)
  {
    _status[thread_id] = std::make_tuple(func, args);
  }

  void Executors::disable(int thread_id)
  {
    _status[thread_id] = std::make_tuple(nullptr, nullptr);
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
      void* args = std::get<1>(_status[id]);

      spdlog::debug("Thread {} begins work! Executing function", id);
      (*ptr)(args);
      spdlog::debug("Thread {} finished work!", id);

      this->disable(id);
      ptr = nullptr;
    } 

  }
}
