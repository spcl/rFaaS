
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>
#include "client.hpp"

namespace client {

  ServerConnection::ServerConnection(const rdmalib::server::ServerStatus & status):
    _status(status),
    _active(_status._address, _status._port)
  {}


  bool ServerConnection::connect()
  {
    _active.allocate();
    return _active.connect();
  }

  void ServerConnection::allocate_send_buffers(int count, uint32_t size)
  {
    for(int i = 0; i < count; ++i) {
      _send.emplace_back(size);
      _send.back().register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE);
    }
  }

  void ServerConnection::allocate_receive_buffers(int count, uint32_t size)
  {
    for(int i = 0; i < count; ++i) {
      _rcv.emplace_back(size);
      _rcv.back().register_memory(_active.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
    }
  }

  rdmalib::Buffer<char> & ServerConnection::send_buffer(int idx)
  {
    assert(idx < _send.size());
    return _send[idx];
  }

  int ServerConnection::submit(int numcores, std::string fname)
  {
    assert(numcores < _send.size() && numcores < _rcv.size());

    int id = 0;

    // 1. Allocate cores TODO
    // currently allocates 0...n-1 cores
    
    // 2. Write recv buffer data TODO

    // 3. Write arguments
    for(int i = 0; i < numcores; ++i) {
      auto & status = _status._buffers[i];
      _active.post_write(_send[i], status.addr, status.rkey);
    }

    // 4. Send execution notification
    //
    return id;
  }

  void ServerConnection::poll_completion(int)
  {

  }

}

