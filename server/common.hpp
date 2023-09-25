
#ifndef __SERVER_COMMON__
#define __SERVER_COMMON__

#include <string>
#include <cstdint>

#include <rdmalib/libraries.hpp>

namespace executor {

  template <typename Library>
  struct ManagerConnection {
    using rkey_t = typename library_traits<Library>::rkey_t;
    std::string addr;
    int port;
    int secret;
    uint64_t r_addr;
    rkey_t r_key;
  };

}

#endif
