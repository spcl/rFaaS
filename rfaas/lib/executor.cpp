
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

  executor::executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg):
    _state(address, port, rcv_buf_size + 1),
    _rcv_buffer(rcv_buf_size),
    _rcv_buf_size(rcv_buf_size),
    _executions(0),
    _max_inlined_msg(max_inlined_msg)
  {
  }

  void executor::allocate(std::string functions_path, int numcores)
  {
    // FIXME: here send cold allocations

    // Load the shared library with functions code
    FILE* file = fopen(functions_path.c_str(), "rb");
    fseek (file, 0 , SEEK_END);
    size_t len = ftell(file);
    rewind(file);
    rdmalib::Buffer<char> functions(len);
    fread(functions.data(), 1, len, file);
    functions.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);

    SPDLOG_DEBUG("Allocating {} threads on a remote executor", numcores);
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
      this->_connections.back().conn->post_send(functions);
      SPDLOG_DEBUG("Connected thread {}/{} and submitted function code.", i + 1, numcores);
      // FIXME: this should be in a function
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
          "Received buffer details for thread, addr {}, rkey {}",
          buf.data()[id].r_addr, buf.data()[id].r_key
        );
        _connections[id].remote_input = rdmalib::RemoteBuffer(buf.data()[id].r_addr, buf.data()[id].r_key);
      }
      received += std::get<1>(wcs);
    }

    // FIXME: Ensure that function code has been submitted
    received = 0;
    while(received < numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
      received += std::get<1>(wcs);
    }
    SPDLOG_DEBUG("Code submission for all threads is finished");
  }

}
