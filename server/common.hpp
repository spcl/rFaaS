
#ifndef __SERVER_COMMON__
#define __SERVER_COMMON__

#include <string>
#include <cstdint>

namespace executor {

  struct ManagerConnection {
    std::string addr;
    int port;
    int secret;
    uint64_t r_addr;
    uint32_t r_key;
  };

}

#endif
