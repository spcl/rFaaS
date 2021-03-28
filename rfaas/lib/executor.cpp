
#include "rdmalib/connection.hpp"
#include <rfaas/executor.hpp>

namespace rfaas {

  executor::executor(std::string address, int port, int rcv_buf_size):
    _state(address, port, rcv_buf_size)
  {

  }

  void executor::allocate(int numcores)
  {
    // FIXME: here send cold allocations

    // Now receive the connections from executors
    for(int i = 0; i < numcores; ++i) {
      this->_connections.emplace_back(
        _state.poll_events([](rdmalib::Connection&){})
      );
    }

    // Now receive buffer information
  }

}
