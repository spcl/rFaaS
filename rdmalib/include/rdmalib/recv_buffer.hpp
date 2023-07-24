
#ifndef __RDMALIB_RECV_BUFFER_HPP__
#define __RDMALIB_RECV_BUFFER_HPP__

#include <optional>
#include <tuple>

#include <rdmalib/connection.hpp>

#include <spdlog/spdlog.h>

namespace rdmalib
{

  template <typename Derived, typename Library>
  struct RecvBuffer
  {
    using wc_t = typename library_traits<Library>::wc_t;
    using Connection_t = typename rdmalib_traits<Library>::Connection;
    //using LibConnection = library_traits<Library>::LibConnection; // TODO

    int _rcv_buf_size;
    int _refill_threshold;
    int _requests;
    constexpr static int DEFAULT_REFILL_THRESHOLD = 8;
    Connection_t *_conn;

    RecvBuffer(int rcv_buf_size) : _rcv_buf_size(rcv_buf_size),
      _refill_threshold(std::min(_rcv_buf_size, DEFAULT_REFILL_THRESHOLD)),
      _requests(0),
      _conn(nullptr)
    {
    }

    inline void connect(Connection_t *conn)
    {
      this->_conn = conn;
      _requests = 0;
      refill();
    }

    inline std::tuple<wc_t *, int> poll(bool blocking = false)
    {
      return static_cast<Derived *>(this)->poll(blocking);
    }

    inline bool refill()
    {
      if (_requests < _refill_threshold)
      {
        SPDLOG_DEBUG("Post {} requests to buffer at QP {}", _rcv_buf_size - _requests, fmt::ptr(_conn->qp()));
        this->_conn->post_batched_empty_recv(_rcv_buf_size - _requests);
        // this->_conn->post_recv({}, -1, _rcv_buf_size - _requests);
        _requests = _rcv_buf_size;
        return true;
      }
      return false;
    }
  };

  struct LibfabricRecvBuffer : RecvBuffer<LibfabricRecvBuffer, libfabric>
  {
    inline std::tuple<wc_t *, int> poll(bool blocking = false)
    {
      auto wc = this->_conn->poll_wc(rdmalib::QueueType::RECV, blocking);
      if (std::get<1>(wc))
        SPDLOG_DEBUG("Polled reqs {}, left {}", std::get<1>(wc), _requests);
      _requests -= std::get<1>(wc);
      return wc;
    }
  };

  struct VerbsRecvBuffer : RecvBuffer<VerbsRecvBuffer, ibverbs>
  {
    inline std::tuple<wc_t *, int> poll(bool blocking = false)
    {
      auto wc = this->_conn->poll_wc(rdmalib::QueueType::RECV, blocking);
      if (std::get<1>(wc))
        SPDLOG_DEBUG("Polled reqs {}, left {}", std::get<1>(wc), _requests);
      _requests -= std::get<1>(wc);
      return wc;
    }
  };
}

#endif
