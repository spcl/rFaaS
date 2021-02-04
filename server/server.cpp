
#include <chrono>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <rdma/rdma_verbs.h>
#include <infiniband/verbs.h>

#include <rdmalib.hpp>


int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
  options.add_options()
    ("a,address", "Use selected address", cxxopts::value<std::string>())
    ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma server!");

  // Start RDMA connection
  rdmalib::Buffer<char> mr(4096);
  rdmalib::RDMAPassive state(opts["address"].as<std::string>(), opts["port"].as<int>());
  state.allocate();
  mr.register_memory(state.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
  auto conn = state.poll_events();
  if(!conn)
    return -1;
 
  memset(mr.data(), 6, 4096);
  state.post_send(*conn, mr);
  state.poll_wc(*conn);
  spdlog::info("Message sent");
  
  memset(mr.data(), 7, 4096);
  state.post_send(*conn, mr);
  state.poll_wc(*conn);
  spdlog::info("Message sent");
  // Start measurement

  // Write request

  // Wait for reading results

  // Stop measurement

  std::this_thread::sleep_for(std::chrono::seconds(5));

  return 0;
}
