
#ifndef __RFAAS_EXECUTOR_HPP__
#define __RFAAS_EXECUTOR_HPP__

#include "rdmalib/buffer.hpp"
#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/rdmalib.hpp>

#include <spdlog/spdlog.h>

namespace rfaas {

  namespace impl {

    template <int I, class... Ts>
    decltype(auto) get(Ts&&... ts) {
        return std::get<I>(std::forward_as_tuple(ts...));
    }

  }

  struct executor_state {
    rdmalib::Connection* conn;
    rdmalib::RemoteBuffer remote_input;
    rdmalib::RecvBuffer _rcv_buffer;
    executor_state(rdmalib::Connection*, int rcv_buf_size);
  };

  struct executor {
    // FIXME: 
    rdmalib::RDMAPassive _state;
    rdmalib::RecvBuffer _rcv_buffer;
    int _rcv_buf_size;
    int _executions;
    // FIXME: global settings
    int _max_inlined_msg;
    std::vector<executor_state> _connections;

    executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg);

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T>
    bool execute(std::string fname, const rdmalib::Buffer<T> & in, rdmalib::Buffer<T> & out)
    {
      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      // FIXME: function ID
      _connections[0].conn->post_write(in, _connections[0].remote_input, 0, in.bytes() <= _max_inlined_msg);
      _connections[0]._rcv_buffer.refill();
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);

      auto wc = _connections[0]._rcv_buffer.poll(true);
      uint32_t val = ntohl(std::get<0>(wc)[0].imm_data);
      if(val == 0)
        return true;
      else {
        if(val == 1)
          spdlog::error("Thread busy, cannot post work");
        else
          spdlog::error("Unknown error {}", val);
        return false;
      }
    }

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T>> & in, std::vector<rdmalib::Buffer<T>> & out)
    {
      int numcores = _connections.size();
      for(int i = 0; i < numcores; ++i) {
        // FIXME: here get a future for async
        char* data = static_cast<char*>(in[i].ptr());
        // TODO: we assume here uintptr_t is 8 bytes
        *reinterpret_cast<uint64_t*>(data) = out[i].address();
        *reinterpret_cast<uint32_t*>(data + 8) = out[i].rkey();
        // FIXME: function ID
        _connections[i].conn->post_write(in[i], _connections[i].remote_input, 0, in[i].bytes() <= _max_inlined_msg);
      }

      for(int i = 0; i < numcores; ++i) {
        _connections[i]._rcv_buffer.refill();
      }
      int expected = numcores;
      while(expected) {
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
        expected -= std::get<1>(wc);
      }

      expected = numcores;
      bool correct = true;
      while(expected) {
        auto wc = _connections[0]._rcv_buffer.poll(true);
        expected -= std::get<1>(wc);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          correct &= val == 0;
        }
      }
      _connections[0]._rcv_buffer._requests += numcores - 1;
      for(int i = 1; i < numcores; ++i)
        _connections[i]._rcv_buffer._requests--;
      return correct;
    }

    void allocate(int numcores);
  };

}

#endif
