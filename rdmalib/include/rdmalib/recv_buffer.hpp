
#ifndef __RDMALIB_RECV_BUFFER_HPP__
#define __RDMALIB_RECV_BUFFER_HPP__

#include <optional>

#include <rdmalib/connection.hpp>

#include <spdlog/spdlog.h>

namespace rdmalib {

  struct RecvBuffer {
    int _rcv_buf_size;
    int _refill_threshold;
    int _requests;
    constexpr static int DEFAULT_REFILL_THRESHOLD = 8;
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
      _requests = 0;
      refill();
    }

    inline std::tuple<ibv_wc*,int> poll(bool blocking = false)
    {
      auto wc = this->_conn->poll_wc(rdmalib::QueueType::RECV, blocking);
      if(std::get<1>(wc))
        SPDLOG_DEBUG("Polled reqs {}, left {}", std::get<1>(wc), _requests);
      _requests -= std::get<1>(wc);
      return wc;
    }

    inline bool refill()
    {
      if(_requests < _refill_threshold) {
        SPDLOG_DEBUG("Post {} requests to buffer at QP {}", _rcv_buf_size - _requests, fmt::ptr(_conn->qp()));
        this->_conn->post_batched_empty_recv(_rcv_buf_size - _requests);
        //this->_conn->post_recv({}, -1, _rcv_buf_size - _requests);
        _requests = _rcv_buf_size;
        return true;
      }
      return false;
    }
  };
}

#endif

