
#ifndef __SERVER_EXECUTOR_MANAGER_ACCOUNTING_HPP__
#define __SERVER_EXECUTOR_MANAGER_ACCOUNTING_HPP__

#include <cstdint>

namespace rfaas::executor_manager {

  // FIXME: Memory accounting for all clients?
  struct Accounting {
    volatile uint64_t hot_polling_time;
    volatile uint64_t execution_time; 
  };

}

#endif

