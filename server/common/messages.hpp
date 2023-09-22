
#ifndef __COMMON_MESSAGES_HPP__
#define __COMMON_MESSAGES_HPP__

#include <cstdint>

namespace rfaas { namespace common {

  struct NodeRegistration {

    static constexpr int NODE_NAME_LENGTH = 32;

    const uint32_t message_id = 1;
    char node_name[NODE_NAME_LENGTH];

  };

  struct LeaseAllocation {

    int memory;
    int cores;
    uint32_t lease_id;

  };

  struct LeaseDeallocation {

    const uint32_t message_id = 2;
    uint32_t lease_id;

  };

}}

#endif
