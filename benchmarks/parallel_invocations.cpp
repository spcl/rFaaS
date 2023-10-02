
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>

#include <rfaas/client.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

#include "parallel_invocations.hpp"
#include "settings.hpp"


int main(int argc, char ** argv)
{
  auto opts = parallel_invocations::options(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test parallel invocations!");

  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);
  in_dev.close();

  // Read benchmark settings
  std::ifstream benchmark_cfg{opts.json_config};
  rfaas::benchmark::Settings settings = rfaas::benchmark::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  // For this benchmark, we override settings.
  if(opts.numcores > 0) {
    settings.benchmark.numcores = opts.numcores;
  }

  rfaas::client instance(
    settings.resource_manager_address, settings.resource_manager_port,
    *settings.device
  );
  if (!instance.connect()) {
    spdlog::error("Connection to resource manager failed!");
    return 1;
  }

  auto leased_executor = instance.lease(settings.benchmark.numcores, settings.benchmark.memory, *settings.device);
  if (!leased_executor.has_value()) {
    spdlog::error("Couldn't acquire a lease!");
    return 1;
  }

  rfaas::executor executor = std::move(leased_executor.value());

  if(!executor.allocate(
    opts.flib,
    opts.input_size,
    settings.benchmark.hot_timeout,
    false
  )) {
    spdlog::error("Connection to executor and allocation failed!");
    return 1;
  }

  // FIXME: move me to allocator
  std::vector<rdmalib::Buffer<char>> in;
  std::vector<rdmalib::Buffer<char>> out;
  for(int i = 0; i < settings.benchmark.numcores; ++i) {
    in.emplace_back(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
    in.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
    memset(in.back().data(), 0, opts.input_size);
    for(int i = 0; i < opts.input_size; ++i) {
      ((char*)in.back().data())[i] = 1;
    }
  }
  for(int i = 0; i < settings.benchmark.numcores; ++i) {
    out.emplace_back(opts.input_size);
    out.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Warmups begin");
  for(int i = 0; i < settings.benchmark.warmup_repetitions; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fname, in, out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for(int i = 0; i < settings.benchmark.repetitions;) {
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
  spdlog::info(
    "Executed {} repetitions, avg {} usec/iter, median {}",
    settings.benchmark.repetitions, avg, median
  );
  if(opts.output_stats != "")
    benchmarker.export_csv(opts.output_stats, {"time"});
  executor.deallocate();

  int i = 0;
  for(rdmalib::Buffer<char> & buf : out) {
    printf("Function %d Data:", i++);
    for(int i = 0; i < std::min(100, opts.input_size); ++i)
      printf("%d ", ((char*)buf.data())[i]);
    printf("\n");
  }

  instance.disconnect();

  return 0;
}
