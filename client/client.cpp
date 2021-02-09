
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include "client.hpp"


int main(int argc, char ** argv)
{
  auto opts = client::options(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma client!");

  std::ifstream in(opts["file"].as<std::string>());
  client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in));
  in.close();
  client.allocate_send_buffers(2, 4096);
  client.allocate_receive_buffers(2, 4096);
  if(!client.connect())
    return -1;
  spdlog::info("Connected to the server!");
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // prepare args
  client.send_buffer(0).data()[0] = 1;
  client.send_buffer(1).data()[0] = 1;

  //client._submit_buffer.data()[0] = 100;
  //client._active.post_send(client._submit_buffer);
  //client._active.poll_wc(rdmalib::QueueType::SEND);
  client.submit(2, "test");
  //client._active.poll_wc(rdmalib::QueueType::SEND);

  // Start RDMA connection
  //rdmalib::Buffer<char> mr(4096), mr2(4096);
  ////rdmalib::RDMAActive active(status._address, status._port);
  //mr.register_memory(client._active.pd(), IBV_ACCESS_LOCAL_WRITE);
  ////mr2.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);

  //client._active.post_recv(mr);
  ////active.post_recv(mr2);
  //client._active.poll_wc(rdmalib::QueueType::RECV);
  //spdlog::info("Received");
  //mr.data()[0] = 100;
  //client._submit_buffer.data()[0] = 100;
  //client._active.post_send(client._submit_buffer);
  //client._active.poll_wc(rdmalib::QueueType::SEND);
  //spdlog::info("Sent");
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

  //memset(mr.data(), 9, 4096);
  //active.post_write(mr, status._buffers[0].addr, status._buffers[0].rkey, 0x1234);
  //active.poll_wc(rdmalib::QueueType::SEND);
  return 0;
}
