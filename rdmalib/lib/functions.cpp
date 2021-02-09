
#include <spdlog/spdlog.h>

#include <rdmalib/functions.hpp>

namespace rdmalib { namespace functions {

  void FunctionsDB::test_function(void* args)
  {
    int val = *static_cast<int*>(args);
    spdlog::debug("Received {}, value {}", fmt::ptr(args), val);
  }

  FunctionsDB::FunctionsDB()
  {
    functions["test"] = test_function;
  }

}}

