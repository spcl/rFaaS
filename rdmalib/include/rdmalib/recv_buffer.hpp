
#ifndef __RDMALIB_RECV_BUFFER_HPP__
#define __RDMALIB_RECV_BUFFER_HPP__

#include <optional>

#include <rdmalib/connection.hpp>

namespace rdmalib {

  struct RecvBuffer {
    int _rcv_buf_size;
    int _refill_threshold;
    int _requests;
    constexpr int DEFAULT_REFILL_THRESHOLD = 8;
    rdmalib::Connection * _conn;

    RecvBuffer(int rcv_buf_size):
      _rcv_buf_size(rcv_buf_size),
      _refill_threshold(std::min(_rcv_buf_size, DEFAULT_REFILL_THRESHOLD)),
      _requests(0),
      _conn(nullptr)
    {}

    inline void connect(rdmalib::Connection * conn)
    {
      this->_conn = conn;
      refill();
    }

    inline ibv_wc* poll(bool blocking = false)
    {
      ibv_wc* wc = this->_conn->poll_wc(rdmalib::QueueType::RECV, blocking);
      _requests -= wc != nullptr;
      return wc;
    }

    inline bool refill()
    {
      if(_requests < _refill_threshold) {
        this->_conn->post_recv({}, -1, _rcv_buf_size - _requests);
        _requests = _rcv_buf_size;
        return true;
      }
      return false;
    }
  };
}

#endif

