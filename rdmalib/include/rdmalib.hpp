
#include <cstdint>

#include <rdma/rdma_cma.h>


namespace rdmalib {

  // Implemented as IPV4
  struct Address {
    sockaddr_in _addr;
    uint16_t _port;

    Address(int port)
    {
      memset(&_addr, 0, sizeof(_addr));
      _addr.sin_family = AF_INET;
      _addr.sin_port = htons(port);
      _port = port;
      printf("%d\n",_addr.sin_port);
    }
  };

  struct RDMAConnection {

    RDMAConnection();

  };

  struct RDMAListen {
    Address _addr;

    RDMAListen(rdma_cm_id * id, int port);
  };

  struct RDMAConnect {

  };

  struct RDMAState {
    rdma_event_channel * _ec;
    rdma_cm_id * _id;

    RDMAState();
    ~RDMAState();

    RDMAListen listen(int port = 0) const;
    RDMAConnect connect() const;
  };
}
