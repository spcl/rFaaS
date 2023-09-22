
#ifndef __RFAAS_EXECUTOR_HPP__
#define __RFAAS_EXECUTOR_HPP__

#include <algorithm>
#include <iterator>
#include <future>
#include <fcntl.h>

#include <rdmalib/benchmarker.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/rdmalib.hpp>

#include <rfaas/connection.hpp>
#include <rfaas/devices.hpp>

#include <spdlog/spdlog.h>

namespace rfaas {

  struct servers;

  namespace impl {

    template <int I, class... Ts>
    decltype(auto) get(Ts&&... ts) {
        return std::get<I>(std::forward_as_tuple(ts...));
    }

  }

  struct polling_type {
    static const polling_type HOT_ALWAYS;
    static const polling_type WARM_ALWAYS;

    int _timeout;

    polling_type(int timeout);
    operator int() const;
  };

  template <typename Library>
  struct executor_state {

    using RemoteBuffer_t = typename rdmalib::rdmalib_traits<Library>::RemoteBuffer;
    using RecvBuffer_t = typename rdmalib::rdmalib_traits<Library>::RecvBuffer;
    using Connection_t = typename rdmalib::rdmalib_traits<Library>::Connection;

    std::unique_ptr<Connection_t> conn;
    RemoteBuffer_t remote_input;
    RecvBuffer_t _rcv_buffer;
    executor_state(Connection_t*, int rcv_buf_size);
  };

  template <typename Derived, typename Library>
  struct executor {

    using RDMAPassive_t = typename rdmalib::rdmalib_traits<Library>::RDMAPassive;
    using RecvBuffer_t = typename rdmalib::rdmalib_traits<Library>::RecvBuffer;
    using ScatterGatherElement_t = typename rdmalib::rdmalib_traits<Library>::ScatterGatherElement;
    using RemoteBuffer_t = typename rdmalib::rdmalib_traits<Library>::RemoteBuffer;
    using rkey_t = typename library_traits<Library>::rkey_t;

    static constexpr int MAX_REMOTE_WORKERS = 64;
    // FIXME: 
    RDMAPassive_t _state;
    RecvBuffer_t _rcv_buffer;
    rdmalib::Buffer<rdmalib::BufferInformation<Library>, Library> _execs_buf;
    std::string _address;
    int _port;
    int _rcv_buf_size;
    int _executions;
    int _invoc_id;
    // FIXME: global settings
    size_t _max_inlined_msg;
    std::vector<executor_state<Library>> _connections;
    std::unique_ptr<manager_connection<Library>> _exec_manager;
    std::vector<std::string> _func_names;
    rdmalib::PerfBenchmarker<8> _perf;

    // manage async executions
    std::atomic<bool> _end_requested;
    std::atomic<bool> _active_polling;
    //std::unordered_map<int, std::promise<int>> _futures;
    std::unordered_map<int, std::tuple<int, std::promise<int>>> _futures;
    std::unique_ptr<std::thread> _background_thread;
    int events;

    executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg);
    executor(device_data & dev);
    ~executor();

    // Skipping managers is useful for benchmarking
    bool allocate(std::string functions_path, int numcores, int max_input_size, int hot_timeout,
        bool skip_manager = false, rdmalib::Benchmarker<5> * benchmarker = nullptr)
    {
      return static_cast<Derived*>(this)->allocate(functions_path, numcores, max_input_size, hot_timeout, skip_manager, benchmarker);
    }
    void deallocate()
    {
      static_cast<Derived*>(this)->deallocate();
    }
    rdmalib::Buffer<char, Library> load_library(std::string path)
    {
      return static_cast<Derived*>(this)->load_library(path);
    }
    void poll_queue()
    {
      static_cast<Derived*>(this)->poll_queue();
    }

    template<typename T, typename U>
    std::future<int> async(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out, int64_t size = -1);

    template<typename T,typename U>
    std::future<int> async(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<U, Library>> & out);
    bool block();

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T, typename U>
    std::tuple<bool, int> execute(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out);

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<T, Library>> & out);
  };

  struct libfabric_executor : executor<libfabric_executor, libfabric> {
    using Library = libfabric;

    rdmalib::Buffer<char, Library> load_library(std::string path);
    void poll_queue();

    libfabric_executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg);

    bool allocate(std::string functions_path, int numcores, int max_input_size,
      int hot_timeout, bool skip_manager, rdmalib::Benchmarker<5> * benchmarker);
    void deallocate();

    template<typename T, typename U>
    std::future<int> async(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out, int64_t size = -1)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint64_t*>(data + 8) = out.rkey();

      int invoc_id = this->_invoc_id++;
      //_futures[invoc_id] = std::move(std::promise<int>{});
      _futures[invoc_id] = std::make_tuple(1, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, submission_id
      );
      if(size != -1) {
        ScatterGatherElement_t sge;
        sge.add(in, size, 0);
        _connections[0].conn->post_write<T>(
          in, 
          size,
          0,
          _connections[0].remote_input,
          submission_id
        );
      } else {
        _connections[0].conn->post_write<T>(
          in,
          in.bytes(),
          0,
          _connections[0].remote_input,
          submission_id
        );
      }
      return std::get<1>(_futures[invoc_id]).get_future();
    }

    template<typename T,typename U>
    std::future<int> async(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<U, Library>> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      int invoc_id = this->_invoc_id++;
      //_futures[invoc_id] = std::move(std::promise<int>{});
      int numcores = _connections.size();
      _futures[invoc_id] = std::make_tuple(numcores, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      for(int i = 0; i < numcores; ++i) {
        // FIXME: here get a future for async
        char* data = static_cast<char*>(in[i].ptr());
        // TODO: we assume here uintptr_t is 8 bytes
        *reinterpret_cast<uint64_t*>(data) = out[i].address();
        #ifdef USE_LIBFABRIC
        *reinterpret_cast<uint64_t*>(data + 8) = out[i].rkey();
        #else
        *reinterpret_cast<uint32_t*>(data + 8) = out[i].rkey();
        #endif

        SPDLOG_DEBUG("Invoke function {} with invocation id {}", func_idx, _invoc_id);
        _connections[i].conn->post_write<T>(
          in[i],
          in[i].bytes(),
          0,
          _connections[i].remote_input,
          submission_id
        );
      }

      return std::get<1>(_futures[invoc_id]).get_future();
    }

    bool block()
    {
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);

      auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true, -1, true);
      uint32_t val = std::get<0>(wc)[0].data;
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

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T, typename U>
    std::tuple<bool, int> execute(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out)
    {
      //_perf.point();
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::make_tuple(false, 0);
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint64_t*>(data + 8) = out.rkey();

      int invoc_id = this->_invoc_id++;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, (invoc_id << 16) | func_idx
      );
      //_perf.point(1);
      _connections[0].conn->post_write(
        in,
        in.bytes(),
        0,
        _connections[0].remote_input,
        (invoc_id << 16) | func_idx
      );
      _active_polling = true;
      //_perf.point(2);
      //_perf.point(3);

      bool found_result = false;
      int return_value = 0;
      int out_size = 0;
      while(!found_result) {
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true, -1, true);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          //_perf.point(4);
          uint64_t val = std::get<0>(wc)[i].data;
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16 & 0x0000FFFF;
          int len = val >> 32;

          if(finished_invoc_id == invoc_id) {
            found_result = true;
            return_value = return_val;
            out_size = len;
            // spdlog::info("Result {} for id {}", return_val, finished_invoc_id);
          } else {
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
        }
        if(found_result) {
          //_perf.point(5);
          _active_polling = false;
          auto wc = _connections[0]._rcv_buffer.poll(false);
          // Catch very unlikely interleaving
          // Event arrives after we poll while the background thread is skipping
          // because we still hold the atomic
          // Thus, we later unset the variable since we're done
          for(int i = 0; i < std::get<1>(wc); ++i) {
            uint32_t val = std::get<0>(wc)[i].data;
            int return_val = val & 0x0000FFFF;
            int finished_invoc_id = val >> 16;
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
          //_perf.point(6);
        }
      }
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
      //_perf.point(7);
      if(return_value == 0) {
        SPDLOG_DEBUG("Finished invocation {} succesfully", invoc_id);
        return std::make_tuple(true, out_size);
      } else {
        if(return_value == 1)
          spdlog::error("Invocation: {}, Thread busy, cannot post work", invoc_id);
        else
          spdlog::error("Invocation: {}, Unknown error {}", invoc_id, return_value);
        return std::make_tuple(false, 0);
      }
    }

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<T, Library>> & out)
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
        *reinterpret_cast<uint64_t*>(data + 8) = out[i].rkey();

        SPDLOG_DEBUG("Invoke function {} with invocation id {}", func_idx, _invoc_id);
        _connections[i].conn->post_write<T>(
          in[i],
          in[i].bytes(),
          0,
          _connections[i].remote_input,
          (_invoc_id++ << 16) | func_idx
        );
      }

      int expected = numcores;
      while(expected) {
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
        expected -= std::get<1>(wc);
      }

      expected = numcores;
      bool correct = true;
      _active_polling = true;
      while(expected) {
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true, -1, true);
        SPDLOG_DEBUG("Found data");
        expected -= std::get<1>(wc);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = std::get<0>(wc)[i].data;
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
      _active_polling = false;

      _connections[0]._rcv_buffer._requests += numcores - 1;
      for(int i = 1; i < numcores; ++i)
        _connections[i]._rcv_buffer._requests--;
      return correct;
    }
  };

  struct verbs_executor : executor<verbs_executor, ibverbs> {
    using Library = ibverbs;

    rdmalib::Buffer<char, Library> load_library(std::string path);
    void poll_queue();
    bool allocate(std::string functions_path, int numcores, int max_input_size,
      int hot_timeout, bool skip_manager, rdmalib::Benchmarker<5> * benchmarker);
    verbs_executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg);
    void deallocate();

    template<typename T, typename U>
    std::future<int> async(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out, int64_t size = -1)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      int invoc_id = this->_invoc_id++;
      //_futures[invoc_id] = std::move(std::promise<int>{});
      _futures[invoc_id] = std::make_tuple(1, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, submission_id
      );
      if(size != -1) {
        ScatterGatherElement_t sge;
        sge.add(in, size, 0);
        _connections[0].conn->post_write(
          std::move(sge),
          _connections[0].remote_input,
          submission_id,
          size <= _max_inlined_msg,
          true
        );
      } else {
        _connections[0].conn->post_write(
          in,
          _connections[0].remote_input,
          submission_id,
          in.bytes() <= _max_inlined_msg,
          true
        );
      }
      _connections[0]._rcv_buffer.refill();
      return std::get<1>(_futures[invoc_id]).get_future();
    }

    template<typename T,typename U>
    std::future<int> async(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<U, Library>> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      int invoc_id = this->_invoc_id++;
      //_futures[invoc_id] = std::move(std::promise<int>{});
      int numcores = _connections.size();
      _futures[invoc_id] = std::make_tuple(numcores, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
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
          submission_id,
          in[i].bytes() <= _max_inlined_msg,
          true
        );
      }

      for(int i = 0; i < numcores; ++i) {
        _connections[i]._rcv_buffer.refill();
      }
      return std::get<1>(_futures[invoc_id]).get_future();
    }

    bool block()
    {
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

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T, typename U>
    std::tuple<bool, int> execute(std::string fname, const rdmalib::Buffer<T, Library> & in, rdmalib::Buffer<U, Library> & out)
    {
      //_perf.point();
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::make_tuple(false, 0);
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
      //_perf.point(1);
      _connections[0].conn->post_write(
        in,
        _connections[0].remote_input,
        (invoc_id << 16) | func_idx,
        in.bytes() <= _max_inlined_msg
      );
      _active_polling = true;
      //_perf.point(2);
      _connections[0]._rcv_buffer.refill();
      //_perf.point(3);

      bool found_result = false;
      int return_value = 0;
      int out_size = 0;
      while(!found_result) {
        auto wc = _connections[0]._rcv_buffer.poll(true);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;

          if(finished_invoc_id == invoc_id) {
            found_result = true;
            return_value = return_val;
            out_size = std::get<0>(wc)[i].byte_len;
            // spdlog::info("Result {} for id {}", return_val, finished_invoc_id);
          } else {
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
        }
        if(found_result) {
          //_perf.point(5);
          _active_polling = false;
          auto wc = _connections[0]._rcv_buffer.poll(false);
          // Catch very unlikely interleaving
          // Event arrives after we poll while the background thread is skipping
          // because we still hold the atomic
          // Thus, we later unset the variable since we're done
          for(int i = 0; i < std::get<1>(wc); ++i) {
            uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
            int return_val = val & 0x0000FFFF;
            int finished_invoc_id = val >> 16;
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
          //_perf.point(6);
        }
      }
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
      //_perf.point(7);
      if(return_value == 0) {
        SPDLOG_DEBUG("Finished invocation {} succesfully", invoc_id);
        return std::make_tuple(true, out_size);
      } else {
        if(return_value == 1)
          spdlog::error("Invocation: {}, Thread busy, cannot post work", invoc_id);
        else
          spdlog::error("Invocation: {}, Unknown error {}", invoc_id, return_value);
        return std::make_tuple(false, 0);
      }
    }

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T, Library>> & in, std::vector<rdmalib::Buffer<T, Library>> & out)
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
      _active_polling = true;
      while(expected) {
        auto wc = _connections[0]._rcv_buffer.poll(true);
        SPDLOG_DEBUG("Found data");
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
      _active_polling = false;

      _connections[0]._rcv_buffer._requests += numcores - 1;
      for(int i = 1; i < numcores; ++i)
        _connections[i]._rcv_buffer._requests--;
      return correct;
    }
  };

}

#endif
