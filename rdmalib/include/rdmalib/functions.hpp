
#include <unordered_map>
#include <string>

namespace rdmalib { namespace functions {

  struct Submission {
    int32_t core_begin, core_end;
    char ID[92];
    static constexpr int DATA_HEADER_SIZE = 16;
  };

  typedef void (*FuncType)(void*, void*);

  struct FunctionsDB {

    std::unordered_map<int32_t, FuncType> functions;
    static void test_function(void* args, void*);

    FunctionsDB();
  };

}}
