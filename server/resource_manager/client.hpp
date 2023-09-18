
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

namespace rfaas {
  struct LeaseRequest;
}

namespace rfaas::resource_manager {

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection* connection;
    rdmalib::Buffer<rfaas::LeaseResponse> _response;
    rdmalib::Buffer<rfaas::LeaseRequest> allocation_requests;
    rdmalib::RecvBuffer rcv_buffer;
    uint32_t allocation_time;
    int client_id;
    std::chrono::high_resolution_clock::time_point _cur_allocation_start;

    Client(int client_id, rdmalib::Connection* conn, ibv_pd* pd);
    rdmalib::Buffer<rfaas::LeaseResponse>& response();
    void begin_allocation();
    void end_allocation();
    void reload_queue();
    void disable();
  };

}

#endif