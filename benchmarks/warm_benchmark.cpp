
#include <chrono>
#include <fstream>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/rdmalib.hpp>

#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>
#include <rfaas/rfaas.hpp>

#include "settings.hpp"
#include "warm_benchmark.hpp"

int main(int argc, char **argv) {
  auto opts = warm_benchmarker::options(argc, argv);
  if (opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test warm_benchmarker!");

  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);
  in_dev.close();

  // Read benchmark settings
  std::ifstream benchmark_cfg{opts.json_config};
  rfaas::benchmark::Settings settings =
      rfaas::benchmark::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  rfaas::client instance(
    settings.resource_manager_address, settings.resource_manager_port,
    *settings.device
  );

  bool skip_resource_manager = !opts.executors_database.empty();

  std::optional<rfaas::executor> leased_executor;
  if (!skip_resource_manager) {

    if (!instance.connect()) {
      spdlog::error("Connection to resource manager failed!");
      return 1;
    }

    leased_executor = instance.lease(settings.benchmark.numcores, settings.benchmark.memory, *settings.device);
    if (!leased_executor.has_value()) {
      spdlog::error("Couldn't acquire a lease!");
      return 1;
    }

  } else {

    std::ifstream in_cfg(opts.executors_database);
    rfaas::servers::deserialize(in_cfg);
    in_cfg.close();

    leased_executor = instance.lease(rfaas::servers::instance(), settings.benchmark.numcores, settings.benchmark.memory);
    if (!leased_executor.has_value()) {
      spdlog::error("Couldn't acquire a lease!");
      return 1;
    }

  }

  rfaas::executor executor = std::move(leased_executor.value());

  if (!executor.allocate(opts.flib, opts.input_size,
                         settings.benchmark.hot_timeout, false, skip_resource_manager)) {
    spdlog::error("Connection to executor and allocation failed!");
    return 1;
  }

  // FIXME: move me to a memory allocator
  rdmalib::Buffer<char> in(opts.input_size,
                           rdmalib::functions::Submission::DATA_HEADER_SIZE),
      out(opts.input_size);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(),
                      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  memset(in.data(), 0, opts.input_size);
  for (int i = 0; i < opts.input_size; ++i) {
    ((char *)in.data())[i] = 1;
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Warmups begin");
  for (int i = 0; i < settings.benchmark.warmup_repetitions; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fname, in, out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for (int i = 0; i < settings.benchmark.repetitions - 1;) {
    benchmarker.start();
    SPDLOG_DEBUG("Submit execution {}", i);
    auto ret = executor.execute(opts.fname, in, out);
    if (std::get<0>(ret)) {
      SPDLOG_DEBUG("Finished execution {} out of {}", i,
                   settings.benchmark.repetitions);
      benchmarker.end(0);
      ++i;
    } else {
      return 1;
    }
  }
  auto [median, avg] = benchmarker.summary();
  spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}",
               settings.benchmark.repetitions, avg, median);
  if (opts.output_stats != "")
    benchmarker.export_csv(opts.output_stats, {"time"});
  executor.deallocate();

  printf("Data: ");
  for (int i = 0; i < std::min(100, opts.input_size); ++i)
    printf("%d ", ((char *)out.data())[i]);
  printf("\n");

  instance.disconnect();

  return 0;
}
