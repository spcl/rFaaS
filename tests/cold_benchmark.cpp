
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/allocation.hpp>
#include <rfaas/connection.hpp>

#include "cold_benchmark.hpp"
#include "rdmalib/connection.hpp"

int main(int argc, char ** argv)
{
  auto opts = cold_benchmarker::opts(argc, argv);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing cold_benchmarker");

  std::ifstream in(opts.server_file);
  auto cfg = rdmalib::server::ServerStatus::deserialize(in);
  in.close();


  // First connection
  client::ServerConnection client(
    cfg,
    opts.recv_buf_size,
    opts.max_inline_data
  );
  if(!client.connect())
    return -1;

  client._allocation_buffer.data()[0] = {1, 1, 1024, 1024, 10002, "192.168.0.12"};
  rdmalib::ScatterGatherElement sge;
  sge.add(client._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client.connection().post_send(sge);
  client.connection().poll_wc(rdmalib::QueueType::SEND, true);
  spdlog::info("Connected to the executor manager!");

  client._allocation_buffer.data()[0] = {-1, 0, 0, 0, 0, ""};
  rdmalib::ScatterGatherElement sge2;
  sge2.add(client._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client.connection().post_send(sge2);

  // Disconnect?
  client.disconnect();

  // Second connection
  client::ServerConnection client2(
    cfg,
    opts.recv_buf_size,
    opts.max_inline_data
  );

  if(!client2.connect())
    return -1;
  client2._allocation_buffer.data()[0] = {1, 1, 1024, 1024, 10002, "192.168.0.12"};
  rdmalib::ScatterGatherElement sge3;
  sge3.add(client2._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client2.connection().post_send(sge3);

  return 0;
}
