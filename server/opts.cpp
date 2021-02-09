
#include <cxxopts.hpp>

namespace server {

  cxxopts::ParseResult opts(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
    options.add_options()
      ("a,address", "Use selected address", cxxopts::value<std::string>())
      ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
      ("n,numcores", "Number of cores", cxxopts::value<int>()->default_value("1"))
      ("f,file", "Output server status.", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    return options.parse(argc, argv);
  }
}

