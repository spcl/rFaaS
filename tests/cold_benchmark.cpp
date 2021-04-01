
#include <chrono>
#include <thread>
#include <string>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/allocation.hpp>

//#include <rfaas/connection.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

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

  // Read connection details to the managers
  std::ifstream in_cfg(opts.server_file);
  rfaas::servers::deserialize(in_cfg);
  in_cfg.close();
  rfaas::servers & cfg = rfaas::servers::instance();

  rfaas::executor executor(opts.address, opts.port, opts.recv_buf_size, opts.max_inline_data);

  spdlog::info("Connecting to the executor manager at !");
  executor.allocate(opts.flib, opts.cores, opts.input_size, opts.hot_timeout, false);

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

  executor.deallocate();

  return 0;
}
