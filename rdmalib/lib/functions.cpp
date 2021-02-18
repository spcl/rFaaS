
#include <spdlog/spdlog.h>

#include <rdmalib/functions.hpp>

namespace rdmalib { namespace functions {

  void FunctionsDB::test_function(void* args, void* res)
  {
    int* src = static_cast<int*>(args), *dest = static_cast<int*>(res);
    //spdlog::debug("Received {}, value {}", fmt::ptr(args), *src);
    //for(int i = 0; i < 100; ++i)
    //*dest++ = *src++;
    *dest = *src;
  }

  FunctionsDB::FunctionsDB()
  {
    functions[1234] = test_function;
  }

}}
