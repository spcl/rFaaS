
#ifndef __SERVER_RESOURCE_MANAGER_CLIENT_HPP__
#define __SERVER_RESOURCE_MANAGER_CLIENT_HPP__

#include <atomic>
#include <cstdint>
#include <chrono>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

#include <rfaas/allocation.hpp>

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::resource_manager {

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection* connection;
    rdmalib::Buffer<rfaas::AllocationRequest> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    uint32_t allocation_time;
    int client_id;
    std::chrono::high_resolution_clock::time_point _cur_allocation_start;

    Client(int client_id, rdmalib::Connection* conn, ibv_pd* pd);
    void begin_allocation();
    void end_allocation();
    void reload_queue();
    void disable();
  };

}

#endif
