
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib.hpp>


int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
  options.add_options()
    ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  //if(opts["verbose"].as<bool>())
  //  spdlog::set_level(spdlog::level::info);
  //else
  //  spdlog::set_level(spdlog::level::warn);
  //spdlog::info("Executing serverless-rdma server! {}", 12);
  //spdlog::warn("Executing serverless-rdma server!");

  // Start RDMA connection
  rdmalib::RDMAState state;
  auto listener = state.listen(opts["port"].as<int>());

  // Start measurement

  // Write request

  // Wait for reading results

  // Stop measurement

  return 0;
}
