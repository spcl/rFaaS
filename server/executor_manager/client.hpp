
#ifndef __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__
#define __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__

#include <atomic>
#include <cstdint>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

#include "accounting.hpp"
#include "executor_process.hpp"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::executor_manager {

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection* connection;
    rdmalib::Buffer<rdmalib::AllocationRequest> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    std::unique_ptr<ActiveExecutor> executor;
    rdmalib::Buffer<Accounting> accounting;
    uint32_t allocation_time;
    bool _active;

    Client(rdmalib::Connection* conn, ibv_pd* pd);
    void reload_queue();
    void disable(int);
    bool active();
  };

}

#endif

