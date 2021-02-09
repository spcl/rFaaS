
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include "client.hpp"


int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-client", "Invoke functions");
  options.add_options()
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("i,invocations", "Invocations", cxxopts::value<int>())
    ("n,name", "Function name", cxxopts::value<std::string>())
    ("f,file", "Server status", cxxopts::value<std::string>())
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma client!");

  std::ifstream in(opts["file"].as<std::string>());
  client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in));
  if(!client.connect())
    return -1;
  in.close();
  spdlog::info("Connected to the server!");

  // Start RDMA connection
  //rdmalib::Buffer<char> mr(4096), mr2(4096);
  //rdmalib::RDMAActive active(status._address, status._port);
  //mr.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  //mr2.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);

  //active.post_recv(mr);
  //active.post_recv(mr2);
  //active.poll_wc(rdmalib::QueueType::RECV);
  //spdlog::info("Received");
  //for(int i = 0; i < 10; ++i)
  //  printf("%d ", mr.data()[i]);
  //printf("\n");

  //active.poll_wc(rdmalib::QueueType::RECV);
  //spdlog::info("Received");
  //for(int i = 0; i < 10; ++i)
  //  printf("%d ", mr2.data()[i]);
  //printf("\n");

  //memset(mr.data(), 8, 4096);
  //active.post_write(mr, status._buffers[0].addr, status._buffers[0].rkey);
  //active.poll_wc(rdmalib::QueueType::SEND);

  //memset(mr.data(), 0, 4096);
  //active.post_atomics(mr, status._buffers[0].addr, status._buffers[0].rkey, 100);
  //active.poll_wc(rdmalib::QueueType::SEND);

  //std::this_thread::sleep_for(std::chrono::seconds(2));

  //memset(mr.data(), 9, 4096);
  //active.post_write(mr, status._buffers[0].addr, status._buffers[0].rkey, 0x1234);
  //active.poll_wc(rdmalib::QueueType::SEND);
  return 0;
}
