
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include "rdmalib/rdmalib.hpp"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/functions.hpp>

#include "functions.hpp"
#include "common.hpp"
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace rdmalib {
  struct RecvBuffer;
}

namespace server {

  struct Accounting {
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
      rdmalib::Connection* mgr_connection, rdmalib::Buffer<uint64_t> & _accounting_buf,
      const executor::ManagerConnection & _mgr_conn,
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
      rdmalib::Connection* mgr_connection, rdmalib::Buffer<uint64_t> & _accounting_buf,
      const executor::ManagerConnection & _mgr_conn,
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

  struct OversubscriptionBuffer {
    uint64_t buffer_addr;
    uint32_t buffer_rkey;
    bool is_active = false;

    rdmalib::Buffer<uint64_t> buffer;
    rdmalib::Connection* mgr_connection = nullptr;

    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Buffer<uint64_t> atomic_results;

    bool enabled()
    {
      return buffer_addr != 0;
    }

    bool active()
    {
      return is_active && enabled();
    }

    bool check_enabled()
    {
      auto wcs = mgr_connection->receive_wcs().poll(false);

      // Message -> oversubscription enabled or disabled
      if(std::get<1>(wcs) > 0) {
        is_active = !is_active;
        mgr_connection->receive_wcs().refill();
        return true;
      }

      return false;
    }

    void initialize(rdmalib::RDMAActive& mgr_connection)
    {
      this->mgr_connection = &mgr_connection.connection();
      buffer = rdmalib::Buffer<uint64_t>(1);

      buffer.register_memory(mgr_connection.pd(), IBV_ACCESS_LOCAL_WRITE);

      mgr_connection.connection().receive_wcs().refill();
    }

    bool get_allocation(int core_num)
    {
      *buffer.data() = 255;

      mgr_connection->post_cas(
        buffer,
        { buffer_addr + 8*core_num, buffer_rkey},
        0,
        1
      ); 

      mgr_connection->poll_wc(rdmalib::QueueType::SEND, true);

      if(*buffer.data() == 0) {
        SPDLOG_DEBUG("Acquiring exclusive access was succesful!");
      } else {
        SPDLOG_DEBUG("Acquiring exclusive access failed.");
      }

      return *buffer.data() == 0;
    }

    void complete(int core_num)
    {
      *buffer.data() = 255;
      mgr_connection->post_cas(
        buffer,
        { buffer_addr + 8*core_num, buffer_rkey},
        1,
        0
      );

      mgr_connection->poll_wc(rdmalib::QueueType::SEND, true);
    }

  };

  // FIXME: is not movable or copyable at the moment
  struct Thread {


    constexpr static int invocation_mask = 0x00007FFF;
    constexpr static int solicited_mask = 0x00008000;
    Functions _functions;
    std::string addr;
    int port;
    uint32_t  max_inline_data;
    int id, repetitions;
    int max_repetitions;
    int _recv_buffer_size;
    uint64_t sum;
    rdmalib::Buffer<char> send, rcv;
    rdmalib::Connection* conn;
    rdmalib::Connection* _mgr_connection;
    const executor::ManagerConnection & _mgr_conn;
    Accounting _accounting;
    rdmalib::Buffer<uint64_t> _accounting_buf;
    // FIXME: Adjust to billing granularity
    constexpr static int HOT_POLLING_VERIFICATION_PERIOD = 10000;
    PollingState _polling_state;

    OversubscriptionBuffer _oversubscribe_buf;

    Thread(std::string addr, int port, int id, int functions_size,
        int buf_size, int recv_buffer_size, int max_inline_data,
        uint64_t oversubscription_buffer_addr,
        uint32_t oversubscription_buffer_rkey,
        const executor::ManagerConnection & mgr_conn):
      _functions(functions_size),
      addr(addr),
      port(port),
      max_inline_data(max_inline_data),
      id(id),
      repetitions(0),
      max_repetitions(0),
      _recv_buffer_size(recv_buffer_size),
      sum(0),
      send(buf_size),
      rcv(buf_size, rdmalib::functions::Submission::DATA_HEADER_SIZE),
      // +1 to handle batching of functions work completions + initial code submission
      conn(nullptr),
      _mgr_conn(mgr_conn),
      _accounting({0,0,0,0}),
      _accounting_buf(1),
      _oversubscribe_buf{oversubscription_buffer_addr, oversubscription_buffer_rkey}
    {
    }

    Accounting::timepoint_t work(int invoc_id, int func_id, bool solicited, uint32_t in_size);
    void hot(uint32_t hot_timeout);
    void warm();
    void thread_work(int timeout);
  };

  struct FastExecutors {

    std::vector<Thread> _threads_data;
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
      uint64_t oversubscription_buffer_addr,
      uint32_t oversubscription_buffer_rkey,
      const executor::ManagerConnection & mgr_conn
    );
    ~FastExecutors();

    void close();
    void allocate_threads(int, int);
  };

}

#endif

