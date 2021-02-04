
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib.hpp>

#include <rdma/rdma_verbs.h>
#include <infiniband/verbs.h>

int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-client", "Invoke functions");
  options.add_options()
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("a,address", "Server address", cxxopts::value<std::string>())
    ("p,port", "Server port", cxxopts::value<int>())
    ("i,invocations", "Invocations", cxxopts::value<int>())
    ("n,name", "Function name", cxxopts::value<std::string>())
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma client!");

  // Start RDMA connection
  rdmalib::Buffer<char> mr(4096);
  rdmalib::RDMAActive active(opts["address"].as<std::string>(), opts["port"].as<int>());
  active.allocate();
  if(!active.connect())
    return -1;

  mr.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  active.post_recv(mr);
  active.poll_wc();

  for(int i = 0; i < 100; ++i)
    printf("%d ", mr.data()[i]);
  printf("\n");
  active.post_recv(mr);
  active.poll_wc();
  for(int i = 0; i < 100; ++i)
    printf("%d ", mr.data()[i]);
  printf("\n");

  // Start measurement

  // Write request

  // Wait for reading results

  // Stop measurement

  return 0;
}
