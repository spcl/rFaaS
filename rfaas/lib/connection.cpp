
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/connection.hpp>

namespace rfaas {

  manager_connection::manager_connection(std::string address, int port,
      int rcv_buf, int max_inline_data):
    _address(address),
    _port(port),
    _active(_address, _port, rcv_buf),
    _rcv_buffer(rcv_buf),
    _allocation_buffer(rcv_buf + 1),
    _max_inline_data(max_inline_data)
  {
    _active.allocate();
  }

  bool manager_connection::connect()
  {
    SPDLOG_DEBUG("Connecting to manager at {}:{}", _address, _port);
    bool ret = _active.connect();
    if(!ret) {
      spdlog::error("Couldn't connect to manager at {}:{}", _address, _port);
      return false;
    }
    _rcv_buffer.connect(&_active.connection());
    _allocation_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE); 
    // Initialize batch receive WCs
    _active.connection().initialize_batched_recv(_allocation_buffer, sizeof(rdmalib::AllocationRequest));
    return ret;
  }

  void manager_connection::disconnect()
  {
    SPDLOG_DEBUG("Disconnecting from manager at {}:{}", _address, _port);
    // Send deallocation request only if we're connected
    if(_active.is_connected()) {
      request() = (rdmalib::AllocationRequest) {-1, 0, 0, 0, 0, 0, 0, ""};
      rdmalib::ScatterGatherElement sge;
      size_t obj_size = sizeof(rdmalib::AllocationRequest);
      sge.add(_allocation_buffer, obj_size, obj_size*_rcv_buffer._rcv_buf_size);
      _active.connection().post_send(sge);
      _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
      _active.disconnect();
    }
  }

  rdmalib::Connection & manager_connection::connection()
  {
    return _active.connection();
  }

  rdmalib::AllocationRequest & manager_connection::request()
  {
    return *(_allocation_buffer.data() + _rcv_buffer._rcv_buf_size);
  }

  bool manager_connection::submit()
  {
    rdmalib::ScatterGatherElement sge;
    size_t obj_size = sizeof(rdmalib::AllocationRequest);
    sge.add(_allocation_buffer, obj_size, obj_size*_rcv_buffer._rcv_buf_size);
    _active.connection().post_send(sge);
    _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
    // FIXME: check failure
    return true;
  }

}

