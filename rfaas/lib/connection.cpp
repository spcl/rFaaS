
#ifndef USE_LIBFABRIC
#include <infiniband/verbs.h>
#endif

#include <rdmalib/buffer.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/connection.hpp>

namespace rfaas {

  template <typename Library>
  manager_connection<Library>::manager_connection(std::string address, int port,
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

  template <typename Library>
  bool manager_connection<Library>::connect()
  {
    SPDLOG_DEBUG("Connecting to manager at {}:{}", _address, _port);
    bool ret = _active.connect();
    if(!ret) {
      spdlog::error("Couldn't connect to manager at {}:{}", _address, _port);
      return false;
    }
    #ifdef USE_LIBFABRIC
    _allocation_buffer.register_memory(_active.pd(), FI_WRITE | FI_REMOTE_WRITE);
    #else
    _allocation_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    #endif
    // Initialize batch receive WCs
    _active.connection().initialize_batched_recv(_allocation_buffer, sizeof(rdmalib::AllocationRequest));
    _rcv_buffer.connect(&_active.connection());
    return ret;
  }

  template <typename Library>
  void manager_connection<Library>::disconnect()
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

  template <typename Library>
  manager_connection<Library>::Connection_t & manager_connection<Library>::connection()
  {
    return _active.connection();
  }

  template <typename Library>
  rdmalib::AllocationRequest & manager_connection<Library>::request()
  {
    return *(_allocation_buffer.data() + _rcv_buffer._rcv_buf_size);
  }

  template <typename Library>
  bool manager_connection<Library>::submit()
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

