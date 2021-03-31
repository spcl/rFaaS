
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/allocation.hpp>
#include <rfaas/connection.hpp>
#include <rfaas/executor.hpp>

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

  std::ifstream in_cfg(opts.server_file);
  auto cfg = rdmalib::server::ServerStatus::deserialize(in_cfg);
  in_cfg.close();

  rfaas::executor executor("148.187.105.20", 10010, opts.recv_buf_size, opts.max_inline_data);

  // First connection
  client::ServerConnection client(
    cfg,
    opts.recv_buf_size,
    opts.max_inline_data
  );
  if(!client.connect())
    return -1;

  spdlog::info("Connected to the executor manager!");
  client._allocation_buffer.data()[0] = (rdmalib::AllocationRequest) {1, 1, 1024, 1024, 10010, {"148.187.105.20"}};
  rdmalib::ScatterGatherElement sge;
  sge.add(client._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client.connection().post_send(sge);
  client.connection().poll_wc(rdmalib::QueueType::SEND, true);
  executor.allocate("examples/libfunctions.so", 1);

  rdmalib::Buffer<char> in(opts.input_size), out(opts.input_size);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  memset(in.data(), 0, opts.input_size);
  for(int i = 0; i < opts.input_size; ++i) {
    ((char*)in.data())[i] = 1;
  }

  rdmalib::Benchmarker<1> benchmarker{opts.repetitions};
  spdlog::info("Warmups begin");
  for(int i = 0; i < opts.warmup_iters; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fname, in, out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for(int i = 0; i < opts.repetitions;) {
    benchmarker.start();
    SPDLOG_DEBUG("Submit execution {}", i);
    if(executor.execute(opts.fname, in, out)) {
      SPDLOG_DEBUG("Finished execution");
      benchmarker.end(0);
      ++i;
    } else {
      continue;
    }
  }
  auto [median, avg] = benchmarker.summary();
  spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}", opts.repetitions, avg, median);

  for(int i = 0; i < std::min(100, opts.input_size); ++i)
    printf("%d ", ((char*)out.data())[i]);
  printf("\n");

  client._allocation_buffer.data()[0] = (rdmalib::AllocationRequest) {-1, 0, 0, 0, 0, ""};
  rdmalib::ScatterGatherElement sge2;
  sge2.add(client._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client.connection().post_send(sge2);

  // Disconnect?
  client.disconnect();
  return 0;

  // Second connection
  client::ServerConnection client2(
    cfg,
    opts.recv_buf_size,
    opts.max_inline_data
  );

  if(!client2.connect())
    return -1;
  client2._allocation_buffer.data()[0] = (rdmalib::AllocationRequest) {1, 1, 1024, 1024, 10002, "192.168.0.12"};
  rdmalib::ScatterGatherElement sge3;
  sge3.add(client2._allocation_buffer, sizeof(rdmalib::AllocationRequest));
  client2.connection().post_send(sge3);

  return 0;
}
