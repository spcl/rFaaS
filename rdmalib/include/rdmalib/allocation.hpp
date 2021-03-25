
#ifndef __RDMALIB_EXECUTOR_MANAGER__
#define __RDMALIB_EXECUTOR_MANAGER__

#include <cstdint>

namespace rdmalib {

  struct AllocationRequest
  {
    // Number of cores to be allocated
    int16_t cores;
    int16_t input_buf_count;
    int32_t input_buf_size; 
    int32_t func_buf_size;
  };

}

#endif

