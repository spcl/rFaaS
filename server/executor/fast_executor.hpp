
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include "rdmalib/benchmarker.hpp"
#include "rdmalib/rdmalib.hpp"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/functions.hpp>

#include "functions.hpp"
#include "common.hpp"
#include "structures.hpp"
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace server {

  template <typename Library>
  struct Accounting {
    using Connection_t = typename rdmalib::rdmalib_traits<Library>::Connection;
    using RecvBuffer_t = typename rdmalib::rdmalib_traits<Library>::RecvBuffer;

    typedef std::chrono::high_resolution_clock clock_t;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> timepoint_t;
    static constexpr long int BILLING_GRANULARITY = std::chrono::duration_cast<std::chrono::nanoseconds>(1s).count();

    uint64_t total_hot_polling_time;
    uint64_t total_execution_time; 
    uint64_t hot_polling_time;
    uint64_t execution_time; 

    inline void update_execution_time(timepoint_t start, timepoint_t end)
    {
      auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
      execution_time += diff;
      total_execution_time += diff;
    }

    inline void send_updated_execution(
      Connection_t* mgr_connection, rdmalib::Buffer<uint64_t, Library> & _accounting_buf,
      const executor::ManagerConnection<Library> & _mgr_conn,
      bool force = false,
      bool wait = true
    )
    {
      if(force || execution_time > BILLING_GRANULARITY) {
        mgr_connection->post_atomic_fadd(
          _accounting_buf,
          { _mgr_conn.r_addr + 8, _mgr_conn.r_key},
          execution_time
        ); 
        //spdlog::error("Send exec {}", execution_time);
        if(wait)
          mgr_connection->poll_wc(rdmalib::QueueType::SEND, true);
        //spdlog::error("Send exec {} done", execution_time);
        execution_time = 0;
      }
    }

    inline uint32_t update_polling_time(timepoint_t start, timepoint_t end)
    {
      uint32_t time_passed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
      hot_polling_time += time_passed;
      total_hot_polling_time += time_passed;

      return time_passed;
    }

    inline void send_updated_polling(
      Connection_t* mgr_connection, rdmalib::Buffer<uint64_t, Library> & _accounting_buf,
      const executor::ManagerConnection<Library> & _mgr_conn,
      bool force = false,
      bool wait = true
    )
    {
      if(force || hot_polling_time > BILLING_GRANULARITY) {
        // Can happen when we didn't got into polling and were stopped right after execution
        if(hot_polling_time == 0)
          return;
        mgr_connection->post_atomic_fadd(
          _accounting_buf,
          { _mgr_conn.r_addr, _mgr_conn.r_key},
          hot_polling_time
        ); 
        //spdlog::error("Send poll {}", hot_polling_time);
        if(wait)
          mgr_connection->poll_wc(rdmalib::QueueType::SEND, true);
        //spdlog::error("Send poll {} done", hot_polling_time);
        hot_polling_time = 0;
      }
    }
  };

  enum class PollingState {
    HOT = 0,
    HOT_ALWAYS,
    WARM,
    WARM_ALWAYS
  };

  // FIXME: is not movable or copyable at the moment
  template <typename Derived, typename Library>
  struct Thread {
    using Connection_t = typename rdmalib::rdmalib_traits<Library>::Connection;
    using RecvBuffer_t = typename rdmalib::rdmalib_traits<Library>::RecvBuffer;
    using Submission_t = typename rdmalib::rdmalib_traits<Library>::Submission;
    using RDMAActive_t = typename rdmalib::rdmalib_traits<Library>::RDMAActive;

    constexpr static int invocation_mask = 0x00007FFF;
    constexpr static int solicited_mask = 0x00008000;
    Functions _functions;
    std::string addr;
    int port;
    uint32_t  max_inline_data;
    int id, repetitions;
    int max_repetitions;
    uint64_t sum;
    rdmalib::Buffer<char, Library> send, rcv;
    RecvBuffer_t wc_buffer;
    Connection_t * conn;
    Connection_t * _mgr_connection;
    const executor::ManagerConnection<Library> & _mgr_conn;
    Accounting<Library> _accounting;
    rdmalib::Buffer<uint64_t, Library> _accounting_buf;
    rdmalib::PerfBenchmarker<9> _perf;
    // FIXME: Adjust to billing granularity
    constexpr static int HOT_POLLING_VERIFICATION_PERIOD = 10000;
    PollingState _polling_state;

    Thread(std::string addr_, int port_, int id_, int functions_size,
        int buf_size, int recv_buffer_size, int max_inline_data_,
        const executor::ManagerConnection<Library> & mgr_conn):
      _functions(functions_size),
      addr(addr_),
      port(port_),
      max_inline_data(max_inline_data_),
      id(id_),
      repetitions(0),
      max_repetitions(0),
      sum(0),
      send(buf_size),
      rcv(buf_size, Submission_t::DATA_HEADER_SIZE),
      // +1 to handle batching of functions work completions + initial code submission
      wc_buffer(recv_buffer_size + 1),
      conn(nullptr),
      _mgr_conn(mgr_conn),
      _accounting({0,0,0,0}),
      _accounting_buf(1),
      _perf(1000)
    {
    }

    typename Accounting<Library>::timepoint_t work(int invoc_id, int func_id, bool solicited, uint32_t in_size)
    {
      return static_cast<Derived*>(this)->work(invoc_id, func_id, solicited, in_size);
    }
    void hot(uint32_t hot_timeout)
    {
      static_cast<Derived*>(this)->hot(hot_timeout);
    }
    void warm()
    {
      static_cast<Derived*>(this)->warm();
    }
    void thread_work(int timeout)
    {
      static_cast<Derived*>(this)->thread_work(timeout);
    }
  };

  struct LibfabricThread : Thread<LibfabricThread, libfabric> {
    using Library = libfabric;
    LibfabricThread(std::string addr_, int port_, int id_, int functions_size,
        int buf_size, int recv_buffer_size, int max_inline_data_,
        const executor::ManagerConnection<Library> & mgr_conn):
        Thread(addr_, port_, id_, functions_size, buf_size, recv_buffer_size, max_inline_data_,
        _mgr_conn) {}

    typename Accounting<Library>::timepoint_t work(int invoc_id, int func_id, bool solicited, uint32_t in_size);
    void hot(int timeout);
    void warm();
    void thread_work(int timeout);
  };

  struct VerbsThread : Thread<VerbsThread, ibverbs> {
    using Library = ibverbs;
    VerbsThread(std::string addr_, int port_, int id_, int functions_size,
        int buf_size, int recv_buffer_size, int max_inline_data_,
        const executor::ManagerConnection<Library> & mgr_conn):
        Thread(addr_, port_, id_, functions_size, buf_size, recv_buffer_size, max_inline_data_,
        _mgr_conn) {}

    typename Accounting<Library>::timepoint_t work(int invoc_id, int func_id, bool solicited, uint32_t in_size);
    void hot(int timeout);
    void warm();
    void thread_work(int timeout);
  };

  template <typename Library>
  struct FastExecutors {

    using Thread_t = typename server_traits<Library>::Thread;
    std::vector<Thread_t> _threads_data;
    std::vector<std::thread> _threads;
    bool _closing;
    int _numcores;
    int _max_repetitions;
    int _warmup_iters;
    int _pin_threads;
    //const ManagerConnection & _mgr_conn;

    FastExecutors(
      std::string client_addr, int port,
      int function_size,
      int numcores,
      int msg_size,
      int recv_buf_size,
      int max_inline_data,
      int pin_threads,
      const executor::ManagerConnection<Library> & mgr_conn
    );
    ~FastExecutors();

    void close();
    void allocate_threads(int, int);
  };

}

#endif

