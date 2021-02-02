
#include <cstdint>

#include <rdma/rdma_cma.h>


namespace rdmalib {
  struct Address {
    sockaddr_in _addr;
    uint16_t _port;

    Address();
  };

  struct RDMAConnection {

    RDMAConnection();

  };
}
