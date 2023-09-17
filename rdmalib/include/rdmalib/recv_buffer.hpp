
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

    RecvBuffer(int rcv_buf_size, rdmalib::Connection* conn):
      _rcv_buf_size(rcv_buf_size),
      _refill_threshold(std::min(_rcv_buf_size, DEFAULT_REFILL_THRESHOLD)),
      _requests(0),
      _conn(conn)
    {
      refill();
    }

    inline void connect(rdmalib::Connection * conn)
    {
      // FIXME:move initialization of batch recv here
      this->_conn = conn;
      _requests = 0;
      refill();
    }

    template<typename T>
    inline void initialize(const rdmalib::Buffer<T>& receive_buffer)
    {
      if(!_conn) {
        return;
      }
      std::cerr << "Initialize batched recv" << std::endl;
      _conn->initialize_batched_recv(receive_buffer, sizeof(typename rdmalib::Buffer<T>::value_type));
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

