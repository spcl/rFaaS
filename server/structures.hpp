
#ifndef __SERVER_STRUCTURES_HPP__
#define __SERVER_STRUCTURES_HPP__

#include "rdmalib/connection.hpp"
#include <cstdint>

#include <rdmalib/buffer.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/connection.hpp>

namespace server {

  struct ThreadStatus {
    rdmalib::functions::FuncType func;
    uint32_t invoc_id;
    rdmalib::Connection * connection;
  };

}

#endif

