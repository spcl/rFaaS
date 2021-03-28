
#ifndef __RFAAS_EXECUTOR_HPP__
#define __RFAAS_EXECUTOR_HPP__

#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/rdmalib.hpp>

namespace rfaas {

  struct executor_state {
    rdmalib::Connection* conn;
    rdmalib::RecvBuffer _rcv_buffer;
  };

  struct executor {
    // FIXME: 
    rdmalib::RDMAPassive _state;
    std::vector<executor_state> _connections;

    executor(std::string address, int port, int rcv_buf_size);

    void allocate(int numcores);
  };

}

#endif
