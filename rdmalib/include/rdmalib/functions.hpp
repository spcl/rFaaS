
#ifndef __RDMALIB_FUNCTIONS_HPP__
#define __RDMALIB_FUNCTIONS_HPP__

#include <unordered_map>
#include <string>

namespace rdmalib { namespace functions {

  struct LibfabricSubmission {
    uint64_t r_address;
    uint64_t r_key;
    static constexpr int DATA_HEADER_SIZE = 16;
  };

  struct VerbsSubmission {
    uint64_t r_address;
    uint32_t r_key;
    static constexpr int DATA_HEADER_SIZE = 12;
  };

  constexpr int LibfabricSubmission::DATA_HEADER_SIZE;
  constexpr int VerbsSubmission::DATA_HEADER_SIZE;

  typedef void (*FuncType)(void*, void*);

  struct FunctionsDB {

    std::unordered_map<int32_t, FuncType> functions;
    static void test_function(void* args, void*);

    FunctionsDB();
  };

}}

#endif

