
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/util.hpp>
#include "client.hpp"

namespace client {

  ServerConnection::ServerConnection(const rdmalib::server::ServerStatus & status):
    _status(status),
    _active(_status._address, _status._port)
  {
    _active.allocate();
    // TODO: QUEUE_MSG_SIZE
    _submit_buffer = std::move(rdmalib::Buffer<char>(100));
    _submit_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE);

    _atomic_buffer = std::move(rdmalib::Buffer<uint64_t>(1));
    _atomic_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE);
  }


  bool ServerConnection::connect()
  {
    return _active.connect();
  }

  void ServerConnection::allocate_send_buffers(int count, uint32_t size)
  {
    for(int i = 0; i < count; ++i) {
      _send.emplace_back(size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
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

  rdmalib::Buffer<char> & ServerConnection::recv_buffer(int idx)
  {
    assert(idx < _rcv.size());
    return _rcv[idx];
  }

  int ServerConnection::submit(int numcores, std::string fname)
  {
    assert(numcores <= _send.size() && numcores <= _rcv.size());

    int id = 0;

    // 1. Allocate cores TODO more than 2 cores
    // currently allocates 0...n-1 cores
    _active.post_atomics(
      _atomic_buffer,
      _status._threads_allocator.addr,
      _status._threads_allocator.rkey,
      0,
      3
    );
    _active.poll_wc(rdmalib::QueueType::SEND);
    if(*_atomic_buffer.data() == 0) {
      spdlog::debug("Allocation succesfull!");
    }
    
    // 2. Write recv buffer data to arguments
    for(int i = 0; i < numcores; ++i) {
      char* data = static_cast<char*>(_send[i].ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = _rcv[i].address();
      *reinterpret_cast<uint32_t*>(data + 8) = _rcv[i].rkey();
      *reinterpret_cast<uint32_t*>(data + 12) = id;
    }

    // 3. Write arguments
    for(int i = 0; i < numcores; ++i) {
      auto & status = _status._buffers[i];
      _active.post_write(_send[i], status.addr, status.rkey);
      // TODO: remove the serialization here
      _active.poll_wc(rdmalib::QueueType::SEND);
    }

    // 4. Write recv for notification
    _active.post_recv({});

    // 5. Send execution notification
    rdmalib::functions::Submission* ptr = ((rdmalib::functions::Submission*)_submit_buffer.data());
    ptr[0].core_begin = 0;
    ptr[0].core_end = 2;
    memcpy(ptr[0].ID, "test", strlen("test") + 1);
    _active.post_send(_submit_buffer);
    _active.poll_wc(rdmalib::QueueType::SEND);
    spdlog::debug("Function execution ID {} scheduled!", id);

    return id;
  }

  void ServerConnection::poll_completion(int)
  {

  }

}

