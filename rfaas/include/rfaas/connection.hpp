
#ifndef __RFAAS_CONNECTION_HPP__
#define __RFAAS_CONNECTION_HPP__

#include <vector>
#include <fstream>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/allocation.hpp>

namespace rfaas {

  template <typename Library>
  struct manager_connection {

    using RDMAActive_t = typename rdmalib::rdmalib_traits<Library>::RDMAActive;
    using RecvBuffer_t = typename rdmalib::rdmalib_traits<Library>::RecvBuffer;
    using Connection_t = typename rdmalib::rdmalib_traits<Library>::Connection;
    using ScatterGatherElement_t = typename rdmalib::rdmalib_traits<Library>::ScatterGatherElement;

    std::string _address;
    int _port;
    rdmalib::Buffer<char, Library> _submit_buffer;
    RDMAActive_t _active;
    RecvBuffer_t _rcv_buffer;
    rdmalib::Buffer<rdmalib::AllocationRequest, Library> _allocation_buffer;
    int _max_inline_data;

    manager_connection(std::string address, int port, int rcv_buf, int max_inline_data);

    Connection_t & connection();
    rdmalib::AllocationRequest & request();
    bool connect();
    void disconnect();
    bool submit();
  };

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
      ScatterGatherElement_t sge;
      size_t obj_size = sizeof(rdmalib::AllocationRequest);
      sge.add(_allocation_buffer, obj_size, obj_size*_rcv_buffer._rcv_buf_size);
      _active.connection().post_send(sge);
      _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
      _active.disconnect();
    }
  }

  template <typename Library>
  typename manager_connection<Library>::Connection_t & manager_connection<Library>::connection()
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
    ScatterGatherElement_t sge;
    size_t obj_size = sizeof(rdmalib::AllocationRequest);
    sge.add(_allocation_buffer, obj_size, obj_size*_rcv_buffer._rcv_buf_size);
    _active.connection().post_send(sge);
    _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
    // FIXME: check failure
    return true;
  }

}

#endif

