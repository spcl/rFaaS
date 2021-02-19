
#include <cxxopts.hpp>

namespace client {

  // Compilation time of client.cpp decreased from 11 to 1.5 seconds!!!

  cxxopts::ParseResult options(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-client", "Invoke functions");
    options.add_options()
      ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
      ("i,invocations", "Invocations", cxxopts::value<int>())
      ("r,repetitions", "Repetitions", cxxopts::value<int>())
      ("x,requests", "Size of recv buffer", cxxopts::value<int>()->default_value("1"))
      ("n,name", "Function name", cxxopts::value<std::string>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("f,file", "Server status", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    return options.parse(argc, argv);
  }  

}
