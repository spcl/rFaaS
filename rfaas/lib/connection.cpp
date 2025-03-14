
#include <infiniband/verbs.h>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>

namespace rfaas {

  manager_connection::manager_connection(std::string address, int port,
      int rcv_buf, int max_inline_data):
    _address(address),
    _port(port),
    _rcv_buf_size(rcv_buf),
    _max_inline_data(max_inline_data),
    _active(_address, _port, rcv_buf),
    _allocation_buffer(sizeof(LeaseStatus)*rcv_buf + sizeof(AllocationRequest))
  {
    _active.allocate();
    _poller.initialize(_active.connection().qp()->recv_cq);
  }

  bool manager_connection::connect()
  {
    SPDLOG_DEBUG("Connecting to manager at {}:{}", _address, _port);
    // tell the executor manager we are a user
    bool ret = _active.connect();
    if(!ret) {
      spdlog::error("Couldn't connect to manager at {}:{}", _address, _port);
      return false;
    }
    _allocation_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE); 

    // Initialize batch receive WCs
    _active.connection().receive_wcs().initialize(_allocation_buffer, sizeof(LeaseStatus));
    return ret;
  }

  void manager_connection::disconnect()
  {
    SPDLOG_DEBUG("Disconnecting from manager at {}:{}", _address, _port);
    // Send deallocation request only if we're connected
    if(_active.is_connected()) {
      request() = (rfaas::AllocationRequest) {-1, 0, 0, 0, 0, 0, 0, ""};
      rdmalib::ScatterGatherElement sge;
      size_t obj_size = sizeof(rfaas::AllocationRequest);
      sge.add(_allocation_buffer, obj_size, sizeof(LeaseStatus)*_rcv_buf_size);
      _active.connection().post_send(sge);
      _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
      _active.disconnect();
    }
  }

  rdmalib::Connection & manager_connection::connection()
  {
    return _active.connection();
  }

  AllocationRequest & manager_connection::request()
  {
    return *reinterpret_cast<AllocationRequest*>(_allocation_buffer.data() + _rcv_buf_size*sizeof(LeaseStatus));
  }

  LeaseStatus* manager_connection::response(int idx)
  {
    return reinterpret_cast<LeaseStatus*>(_allocation_buffer.data() + idx * sizeof(LeaseStatus));
  }

  LeaseStatus* manager_connection::poll_response()
  {
    auto [wcs, count] = _poller.poll(true);
    if(count > 1) {
      spdlog::warn("Ignoring {} responses from executor manager", count - 1);
    }
    if(wcs[0].status != IBV_WC_SUCCESS) {
      spdlog::error("Couldn't receive the response from executor manager!");
      return nullptr;
    }
    return response(wcs[0].wr_id);
  }

  bool manager_connection::submit()
  {
    rdmalib::ScatterGatherElement sge;
    size_t obj_size = sizeof(AllocationRequest);
    sge.add(_allocation_buffer, obj_size, sizeof(LeaseStatus)*_rcv_buf_size);
    _active.connection().post_send(sge);
    _active.connection().poll_wc(rdmalib::QueueType::SEND, true);

    auto response = poll_response();
    if(!response) {
      return false;
    }

    if(response->status == LeaseStatus::ALLOCATED) {
      return true;
    }

    switch(response->status)
    {
      case LeaseStatus::UNKNOWN:
        spdlog::error("Executor manager doesn't know this lease!");
      break;
      case LeaseStatus::FAILED_ALLOCATE:
        spdlog::error("Executor manager couldn't allocate this lease!");
      break;
      case LeaseStatus::TERMINATING:
        spdlog::error("Lease is being terminated!");
      break;
      case LeaseStatus::FAILED:
        spdlog::error("Executor failed!");
      break;
    }
    return false;
  }

  resource_mgr_connection::resource_mgr_connection(std::string address, int port,
      int rcv_buf, int max_inline_data):
    _address(address),
    _port(port),
    _rcv_buf_size(rcv_buf),
    _send_buffer(1),
    _receive_buffer(rcv_buf),
    _max_inline_data(max_inline_data)
  {
    if(!address.empty()) {
      _active = rdmalib::RDMAActive(_address, _port, rcv_buf);
      _active.allocate();
    }
  }

  bool resource_mgr_connection::connect()
  {
    SPDLOG_DEBUG("Connecting to resource manager at {}:{}", _address, _port);

    uint32_t secret = (CLIENT_ID << 24);
    bool ret = _active.connect(secret);
    if(!ret) {
      spdlog::error("Couldn't connect to manager at {}:{}", _address, _port);
      return false;
    }
    _send_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE); 
    _receive_buffer.register_memory(_active.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE); 

    // Initialize batch receive WCs
    _active.connection().receive_wcs().initialize(_receive_buffer);
    return ret;
  }

  void resource_mgr_connection::disconnect()
  {
    SPDLOG_DEBUG("Disconnecting from manager at {}:{}", _address, _port);

    // Send deallocation request only if we're connected
    if(_active.is_connected()) {
      request() = (rfaas::LeaseRequest) {-1, 0};
      rdmalib::ScatterGatherElement sge;
      size_t obj_size = sizeof(rfaas::LeaseRequest);
      sge.add(_send_buffer, obj_size, 0);
      _active.connection().post_send(sge);
      _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
      _active.disconnect();
    }
  }

  rdmalib::Connection & resource_mgr_connection::connection()
  {
    return _active.connection();
  }

  rfaas::LeaseRequest & resource_mgr_connection::request()
  {
    return *(_send_buffer.data());
  }

  const rfaas::LeaseResponse& resource_mgr_connection::response(int idx) const
  {
    return _receive_buffer.data()[idx];
  }

  bool resource_mgr_connection::submit()
  {
    rdmalib::ScatterGatherElement sge;
    size_t obj_size = sizeof(rfaas::LeaseRequest);
    sge.add(_send_buffer, obj_size, 0);
    _active.connection().post_send(sge);
    _active.connection().poll_wc(rdmalib::QueueType::SEND, true);
    // FIXME: check failure
    // FIXME: receive here details of connection
    return true;
  }

  bool resource_mgr_connection::connected() const
  {
    return _active.is_connected();
  }

}

