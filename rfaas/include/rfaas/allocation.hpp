
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
    static constexpr int MAX_NODES_PER_LEASE = 8;

    LeasedNode nodes[MAX_NODES_PER_LEASE];
  };

  struct AllocationRequest {
    int16_t hot_timeout;
    int16_t timeout;
    // > 0: Number of cores to be allocated
    // < 0: client_id with negative sign, deallocation & disconnect request
    int16_t cores;
    int32_t memory;
    int16_t input_buf_count;
    int32_t input_buf_size;
    uint32_t func_buf_size;
    int32_t listen_port;
    char listen_address[16];
  };

} // namespace rdmalib

#endif
