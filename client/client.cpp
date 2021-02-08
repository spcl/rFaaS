
#include <thread>

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
    ("r,rkey", "Invocations", cxxopts::value<int>())
    ("m,addr", "Invocations", cxxopts::value<uintptr_t>())
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
  rdmalib::Buffer<char> mr(4096), mr2(4096);
  rdmalib::RDMAActive active(opts["address"].as<std::string>(), opts["port"].as<int>());
  active.allocate();
  mr.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  mr2.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  if(!active.connect())
    return -1;

  active.post_recv(mr);
  active.post_recv(mr2);
  active.poll_wc(rdmalib::QueueType::RECV);
  spdlog::info("Received");
  for(int i = 0; i < 10; ++i)
    printf("%d ", mr.data()[i]);
  printf("\n");

  active.poll_wc(rdmalib::QueueType::RECV);
  spdlog::info("Received");
  for(int i = 0; i < 10; ++i)
    printf("%d ", mr2.data()[i]);
  printf("\n");

  memset(mr.data(), 8, 4096);
  active.post_write(mr, opts["addr"].as<uintptr_t>(), opts["rkey"].as<int>());
  active.poll_wc(rdmalib::QueueType::SEND);

  memset(mr.data(), 0, 4096);
  active.post_atomics(mr, opts["addr"].as<uintptr_t>(), opts["rkey"].as<int>(), 100);
  active.poll_wc(rdmalib::QueueType::SEND);

  std::this_thread::sleep_for(std::chrono::seconds(2));

  memset(mr.data(), 9, 4096);
  active.post_write(mr, opts["addr"].as<uintptr_t>(), opts["rkey"].as<int>(), 0x1234);
  active.poll_wc(rdmalib::QueueType::SEND);
  return 0;
}
