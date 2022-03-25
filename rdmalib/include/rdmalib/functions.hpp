
#ifndef __RDMALIB_FUNCTIONS_HPP__
#define __RDMALIB_FUNCTIONS_HPP__

#include <unordered_map>
#include <string>

namespace rdmalib { namespace functions {

  struct Submission {
    uint64_t r_address;
    #ifdef USE_LIBFABRIC
    uint64_t r_key;
    #else
    uint32_t r_key;
    #endif
    static constexpr int DATA_HEADER_SIZE = 12;
  };

  constexpr int Submission::DATA_HEADER_SIZE;


  typedef void (*FuncType)(void*, void*);

  struct FunctionsDB {

    std::unordered_map<int32_t, FuncType> functions;
    static void test_function(void* args, void*);

    FunctionsDB();
  };

}}

#endif

