
#ifndef __COMMON_MESSAGES_HPP__
#define __COMMON_MESSAGES_HPP__

#include <cstdint>
#include <type_traits>

namespace rfaas { namespace common {

  enum class MessageIDs: uint32_t {
    NODE_REGISTRATION = 1,
    LEASE_ALLOCATION = 2,
    LEASE_DEALLOCATION = 3
  };

  enum class LeaseID: int32_t {
    TERMINATE = -1
  };

  constexpr auto id_to_int(MessageIDs id) noexcept
  {
    return static_cast<std::underlying_type_t<MessageIDs>>(id);
  }

  constexpr auto id_to_int(LeaseID id) noexcept
  {
    return static_cast<std::underlying_type_t<MessageIDs>>(id);
  }

  struct NodeRegistration {

    static constexpr int NODE_NAME_LENGTH = 32;

    const uint32_t message_id = id_to_int(MessageIDs::NODE_REGISTRATION);
    char node_name[NODE_NAME_LENGTH];

  };

  struct LeaseAllocation {

    const uint32_t message_id = id_to_int(MessageIDs::LEASE_ALLOCATION);
    int memory;
    int cores;
    int32_t lease_id;

  };

  struct LeaseDeallocation {

    uint32_t message_id = id_to_int(MessageIDs::LEASE_DEALLOCATION);
    int32_t lease_id;
    uint64_t allocation_time;
    uint64_t hot_polling_time;
    uint64_t execution_time;

  };

}}

#endif
