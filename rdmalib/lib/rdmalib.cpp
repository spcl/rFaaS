
#include <cassert>

// inet_ntoa
#include <arpa/inet.h>

#include <rdmalib.hpp>

namespace rdmalib {

  RDMAState::RDMAState()
  {
    this->_ec = rdma_create_event_channel();
    assert(this->_ec);
    // TODO: do we really want TCP-like communication here?
    assert(!rdma_create_id(this->_ec, &this->_id, nullptr, RDMA_PS_TCP));
  }

  RDMAState::~RDMAState()
  {
    rdma_destroy_id(this->_id);
    rdma_destroy_event_channel(this->_ec);
  }

  RDMAListen::RDMAListen(rdma_cm_id * id, int port):
    _addr(port)
  {
    // The cast is safe.
    // https://stackoverflow.com/questions/18609397/whats-the-difference-between-sockaddr-sockaddr-in-and-sockaddr-in6
    assert(!rdma_bind_addr(id, reinterpret_cast<sockaddr*>(&this->_addr._addr)));
    assert(!rdma_listen(id, 10));

    this->_addr._port = ntohs(rdma_get_src_port(id));
  }

  RDMAListen RDMAState::listen(int port) const
  {
    return RDMAListen(this->_id, port);
  }

  RDMAConnect RDMAState::connect() const
  {}
}
