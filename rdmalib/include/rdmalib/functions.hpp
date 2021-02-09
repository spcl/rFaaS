
#include <unordered_map>
#include <string>


namespace rdmalib { namespace functions {

  typedef void (*FuncType)(void*);

  struct FunctionsDB {

    std::unordered_map<std::string, FuncType> functions;
    static void test_function(void* args);

    FunctionsDB();
  };

}}
