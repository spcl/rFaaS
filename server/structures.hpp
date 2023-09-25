
#ifndef __SERVER_STRUCTURES_HPP__
#define __SERVER_STRUCTURES_HPP__

#include "rdmalib/connection.hpp"
#include <cstdint>

#include <rdmalib/rdmalib.hpp>

namespace server {

  template <typename Library>
  struct ThreadStatus {
    using Connection_t = typename rdmalib::rdmalib_traits<Library>::Connection;
    rdmalib::functions::FuncType func;
    uint32_t invoc_id;
    Connection_t * connection;
  };

  template <typename Library>
  struct server_traits;

  // Forward declare
  struct LibfabricThread;
  struct VerbsThread;

  template <>
  struct server_traits<libfabric> {
    using Thread = LibfabricThread;
  };

  template <>
  struct server_traits<ibverbs> {
    using Thread = VerbsThread;
  };

}

#endif

