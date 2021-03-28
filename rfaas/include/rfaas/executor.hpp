
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
    std::vector<executor_state> _connections;

    executor(std::string address, int port, int rcv_buf_size);

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
      _connections[0].conn->post_write(in, _connections[0].remote_input, 0);
      _connections[0]._rcv_buffer.refill();

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

    void allocate(int numcores);
  };

}

#endif
