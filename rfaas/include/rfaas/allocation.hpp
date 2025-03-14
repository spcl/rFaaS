
#ifndef __RFAAS_ALLOCATION_HPP__
#define __RFAAS_ALLOCATION_HPP__

#include <cstdint>

namespace rfaas {

  struct LeaseRequest {
    // > 0: Number of cores to be allocated
    // < 0: client_id with negative sign, deallocation & disconnect request
    int16_t cores;
    int32_t memory;
  };

  struct LeasedNode {
    int32_t port;
    int16_t cores;
    char address[16];
  };

  struct LeaseResponse {

    int32_t lease_id;
    int32_t port;
    char address[16];
    //LeasedNode nodes[MAX_NODES_PER_LEASE];
  };

  struct AllocationRequest {
    // > 0: Lease identificator
    // < 0: client_id with negative sign, deallocation & disconnect request
    int32_t lease_id;
    //uint32_t lease_id;
    int16_t hot_timeout;
    int16_t timeout;
    int16_t input_buf_count;
    int32_t input_buf_size;
    uint32_t func_buf_size;
    int32_t listen_port;
    char listen_address[16];

    // Legacy support for skipping resource manager
    int16_t cores = 0;
    int32_t memory = 0;
  };

  struct LeaseStatus {
    static constexpr int ALLOCATED = 0;
    static constexpr int UNKNOWN = 1;
    static constexpr int FAILED_ALLOCATE = 2;
    static constexpr int TERMINATING = 3;
    static constexpr int FAILED = 4;
    // = 0: Lease allocated
    // = 1: Lease is not known
    // = 3: Lease couldn't be allocated
    // = 4: Lease will be terminated soon
    // = 5: Executor crashed
    int32_t status;
  };

} // namespace rdmalib

#endif
