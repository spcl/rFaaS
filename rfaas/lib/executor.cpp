
#include <spdlog/spdlog.h>

#include <rdmalib/allocation.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/buffer.hpp>
#include <rfaas/executor.hpp>

namespace rfaas {

  executor_state::executor_state(rdmalib::Connection* conn, int rcv_buf_size):
    conn(conn),
    _rcv_buffer(rcv_buf_size)
  {

  }

  executor::executor(std::string address, int port, int rcv_buf_size):
    _state(address, port, rcv_buf_size),
    _rcv_buffer(rcv_buf_size),
    _rcv_buf_size(rcv_buf_size),
    _executions(0)
  {
  }

  void executor::allocate(int numcores)
  {
    // FIXME: here send cold allocations

    // FIXME: temporary fix of vector reallocation - return Connection object?
    _state._connections.reserve(numcores);
    // Now receive the connections from executors
    rdmalib::Buffer<rdmalib::BufferInformation> buf(numcores);
    uint32_t obj_size = sizeof(rdmalib::BufferInformation);
    buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    // FIXME: shared receive queue here - batched receive
    for(int i = 0; i < numcores; ++i) {
      // FIXME: single QP
      this->_connections.emplace_back(
        _state.poll_events(
          [this,&buf,obj_size,i](rdmalib::Connection& conn){
            conn.post_recv(buf.sge(obj_size, i*obj_size), i);
          }
        ),
        _rcv_buf_size
      );
      this->_connections.back()._rcv_buffer.connect(this->_connections.back().conn);
    }

    // Now receive buffer information
    int received = 0;
    while(received < numcores) {
      // FIXME: single QP
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true); 
      for(int i = 0; i < std::get<1>(wcs); ++i) {
        int id = std::get<0>(wcs)[i].wr_id;
        SPDLOG_DEBUG(
          "Received buffer details for thread {}, addr {}, rkey {}",
          i, buf.data()[id].r_addr, buf.data()[id].r_key
        );
        _connections[id].remote_input = rdmalib::RemoteBuffer(buf.data()[id].r_addr, buf.data()[id].r_key);
      }
      received += std::get<1>(wcs);
    }
  }

}
