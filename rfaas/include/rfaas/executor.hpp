
#ifndef __RFAAS_EXECUTOR_HPP__
#define __RFAAS_EXECUTOR_HPP__

#include <algorithm>
#include <iterator>

#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/rdmalib.hpp>

#include <rfaas/connection.hpp>

#include <spdlog/spdlog.h>

namespace rfaas {

  struct servers;

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
    std::string _address;
    int _port;
    int _rcv_buf_size;
    int _executions;
    int _invoc_id;
    // FIXME: global settings
    int _max_inlined_msg;
    std::vector<executor_state> _connections;
    std::unique_ptr<manager_connection> _exec_manager;
    std::vector<std::string> _func_names;

    executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg);

    // Skipping managers is useful for benchmarking
    void allocate(std::string functions_path, int numcores, int max_input_size, int hot_timeout,
        bool skip_manager = false);
    void deallocate();
    rdmalib::Buffer<char> load_library(std::string path);

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T>
    bool execute(std::string fname, const rdmalib::Buffer<T> & in, rdmalib::Buffer<T> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return false;
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      int invoc_id = this->_invoc_id++;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, (invoc_id << 16) | func_idx
      );
      _connections[0].conn->post_write(
        in,
        _connections[0].remote_input,
        (invoc_id << 16) | func_idx,
        in.bytes() <= _max_inlined_msg
      );
      _connections[0]._rcv_buffer.refill();
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);

      auto wc = _connections[0]._rcv_buffer.poll(true);
      uint32_t val = ntohl(std::get<0>(wc)[0].imm_data);
      int return_val = val & 0x0000FFFF;
      int finished_invoc_id = val >> 16;
      if(return_val == 0) {
        SPDLOG_DEBUG("Finished invocation {} succesfully", finished_invoc_id);
        return true;
      } else {
        if(val == 1)
          spdlog::error("Invocation: {}, Thread busy, cannot post work", finished_invoc_id);
        else
          spdlog::error("Invocation: {}, Unknown error {}", finished_invoc_id, val);
        return false;
      }
    }

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T>> & in, std::vector<rdmalib::Buffer<T>> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return false;
      }
      int func_idx = std::distance(_func_names.begin(), it);

      int numcores = _connections.size();
      for(int i = 0; i < numcores; ++i) {
        // FIXME: here get a future for async
        char* data = static_cast<char*>(in[i].ptr());
        // TODO: we assume here uintptr_t is 8 bytes
        *reinterpret_cast<uint64_t*>(data) = out[i].address();
        *reinterpret_cast<uint32_t*>(data + 8) = out[i].rkey();

        SPDLOG_DEBUG("Invoke function {} with invocation id {}", func_idx, _invoc_id);
        _connections[i].conn->post_write(
          in[i],
          _connections[i].remote_input,
          (_invoc_id++ << 16) | func_idx,
          in[i].bytes() <= _max_inlined_msg
        );
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
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;
          if(return_val == 0) {
            SPDLOG_DEBUG("Finished invocation {} succesfully", finished_invoc_id);
          } else {
            if(val == 1)
              spdlog::error("Invocation: {}, Thread busy, cannot post work", finished_invoc_id);
            else
              spdlog::error("Invocation: {}, Unknown error {}", finished_invoc_id, val);
          }
          correct &= return_val == 0;
        }
      }

      _connections[0]._rcv_buffer._requests += numcores - 1;
      for(int i = 1; i < numcores; ++i)
        _connections[i]._rcv_buffer._requests--;
      return correct;
    }
  };

}

#endif
