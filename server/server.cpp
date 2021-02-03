
#include <cxxopts.hpp>

#include <rdmalib.hpp>


int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-client", "Invoke functions");
  options.add_options()
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);

  // Start RDMA connection
  rdmalib::RDMAState state;
  auto listener = state.listen(opts["port"].as<int>());

  // Start measurement

  // Write request

  // Wait for reading results

  // Stop measurement

  return 0;
}
