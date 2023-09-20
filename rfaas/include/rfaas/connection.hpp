
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

  template <typename Library>
  struct manager_connection {

    using RDMAActive_t = typename rdmalib_traits<Library>::RDMAActive;
    using RecvBuffer_t = typename rdmalib_traits<Library>::RecvBuffer;
    using Connection_t = typename rdmalib_traits<Library>::Connection;
    using SGE_t = typename rdmalib_traits<Library>::ScatterGatherElement;

    std::string _address;
    int _port;
    rdmalib::Buffer<char, Library> _submit_buffer;
    RDMAActive_t _active;
    RecvBuffer_t _rcv_buffer;
    rdmalib::Buffer<rdmalib::AllocationRequest, Library> _allocation_buffer;
    int _max_inline_data;

    manager_connection(std::string address, int port, int rcv_buf, int max_inline_data);

    Connection_t & connection();
    rdmalib::AllocationRequest & request();
    bool connect();
    void disconnect();
    bool submit();
  };

}

#endif

