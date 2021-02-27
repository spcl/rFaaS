
#ifndef __SERVER_WC_BUFFER_HPP__
#define __SERVER_WC_BUFFER_HPP__

#include <optional>

#include <rdmalib/connection.hpp>

namespace server {

  struct WCBuffer {
    int _rcv_buf_size;
    int _requests;
    rdmalib::Connection * _conn;

    WCBuffer(int rcv_buf_size):
      _rcv_buf_size(rcv_buf_size),
      _requests(0),
      _conn(nullptr)
    {}

    inline void connect(rdmalib::Connection * conn)
    {
      this->_conn = conn;
      refill();
    }

    inline std::optional<ibv_wc> poll()
    {
      std::optional<ibv_wc> wc = this->_conn->poll_wc(rdmalib::QueueType::RECV, false);
      _requests -= wc.has_value();
      return wc;
    }

    inline void refill()
    {
      if(_requests < 5) {
        this->_conn->post_recv({}, -1, _rcv_buf_size - _requests);
        _requests = _rcv_buf_size;
      }
    }
  };
}

#endif

