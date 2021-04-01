
#ifndef __RFAAS_CONNECTION_HPP__
#define __RFAAS_CONNECTION_HPP__

#include <vector>
#include <fstream>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/allocation.hpp>

namespace rfaas {

  struct manager_connection {
    std::string _address;
    int _port;
    rdmalib::Buffer<char> _submit_buffer;
    rdmalib::RDMAActive _active;
    rdmalib::RecvBuffer _rcv_buffer;
    rdmalib::Buffer<rdmalib::AllocationRequest> _allocation_buffer;
    int _max_inline_data;

    manager_connection(std::string address, int port, int rcv_buf, int max_inline_data);

    rdmalib::Connection & connection();
    rdmalib::AllocationRequest & request();
    bool connect();
    void disconnect();
    bool submit();
  };

}

#endif

