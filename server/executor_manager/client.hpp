
#ifndef __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__
#define __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__

#include <atomic>
#include <cstddef>
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
    #ifdef USE_LIBFABRIC
    std::unique_ptr<ActiveExecutor> executor = nullptr;
    #else
    std::unique_ptr<ActiveExecutor> executor;
    #endif
    rdmalib::Buffer<Accounting> accounting;
    uint32_t allocation_time;
    bool _active;

    #ifdef USE_LIBFABRIC
    Client(rdmalib::Connection* conn, fid_domain* pd);
    #else
    Client(rdmalib::Connection* conn, ibv_pd* pd);
    #endif
    void reload_queue();
    void disable(int);
    bool active();
  };

}

#endif

