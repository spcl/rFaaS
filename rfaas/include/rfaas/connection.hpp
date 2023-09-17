
#ifndef __RFAAS_CONNECTION_HPP__
#define __RFAAS_CONNECTION_HPP__

#include <fstream>
#include <vector>

#include <rdmalib/buffer.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/server.hpp>

#include <rfaas/allocation.hpp>

namespace rfaas {

  struct manager_connection {
    std::string _address;
    int _port;
    rdmalib::Buffer<char> _submit_buffer;
    rdmalib::RDMAActive _active;
    rdmalib::RecvBuffer _rcv_buffer;
    rdmalib::Buffer<rfaas::AllocationRequest> _allocation_buffer;
    int _max_inline_data;

    manager_connection(std::string address, int port, int rcv_buf,
                      int max_inline_data);

    rdmalib::Connection &connection();
    rfaas::AllocationRequest &request();
    bool connect();
    void disconnect();
    bool submit();
  };

  struct resource_mgr_connection {

    //static constexpr int MAX_EXECUTORS_PER_LEASE = 8;
    //typedef std::array<rfaas::LeaseRequest, MAX_EXECUTORS_PER_LEASE> lease_t;

    std::string _address;
    int _port;
    rdmalib::RDMAActive _active;
    rdmalib::RecvBuffer _rcv_buffer;
    rdmalib::Buffer<rfaas::LeaseRequest> _send_buffer;
    rdmalib::Buffer<rfaas::LeaseResponse> _receive_buffer;
    int _max_inline_data;

    resource_mgr_connection(std::string address, int port, int rcv_buf,
                      int max_inline_data);

    rdmalib::Connection &connection();
    rfaas::LeaseRequest &request();
    const rfaas::LeaseResponse& response(int idx) const;
    bool connect();
    bool connected() const;
    void disconnect();
    bool submit();
  };

} // namespace rfaas

#endif
