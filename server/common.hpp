
#ifndef __SERVER_COMMON__
#define __SERVER_COMMON__

#include <string>
#include <cstdint>

namespace executor {

  struct ManagerConnection {
    std::string addr;
    int port;
    int secret;
    int64_t r_addr;
    int32_t r_key;
  };

}

#endif
