
#include <chrono>
#include <thread>
#include <string>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>

#include <rfaas/allocation.hpp>
#include <rfaas/client.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

#include "cold_benchmark.hpp"
#include "settings.hpp"

int main(int argc, char ** argv)
{
  auto opts = cold_benchmarker::opts(argc, argv);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma test cold_benchmarker");
 
  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);
  in_dev.close();

  // Read benchmark settings
  std::ifstream benchmark_cfg{opts.json_config};
  rfaas::benchmark::Settings settings = rfaas::benchmark::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  rfaas::client instance(
    settings.resource_manager_address, settings.resource_manager_port,
    *settings.device
  );
  if (!instance.connect()) {
    spdlog::error("Connection to resource manager failed!");
    return 1;
  }



  rdmalib::Benchmarker<5> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Measurements begin");
  auto start = std::chrono::high_resolution_clock::now();
  for(int i = 0; i < settings.benchmark.repetitions;++i) {

    // Allow the lease information to be propagated
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    spdlog::info("Begin iteration {}", i);

    auto leased_executor = instance.lease(settings.benchmark.numcores, settings.benchmark.memory, *settings.device);
    if (!leased_executor.has_value()) {
      spdlog::error("Couldn't acquire a lease!");
      return 1;
    }
    rfaas::executor executor = std::move(leased_executor.value());

    // Allow the lease information to be propagated
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

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
      memset(out.back().data(), 0, opts.input_size);
    }

    if(executor.allocate(
      opts.flib, opts.input_size, 
      settings.benchmark.hot_timeout, false, &benchmarker
    )) {
      executor.execute(opts.fname, in, out);
      // End of function execution
      benchmarker.end(4);
      executor.deallocate();
    } else {
      benchmarker.remove_last();
      spdlog::error("Allocation not succesfull");
    }
    if(opts.pause > 0) {
      spdlog::info("Sleep between iterations");
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.pause));
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  spdlog::info(
    "Measurements end repetitions {} time {} ms",
    benchmarker._measurements.size(),
    std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() / 1000.0
  );

  auto [median, avg] = benchmarker.summary();
  spdlog::info(
    "Executed {} repetitions, avg {} usec/iter, median {}",
    settings.benchmark.repetitions, avg, median
  );
  if(opts.output_stats != "")
    benchmarker.export_csv(
      opts.output_stats,
      {"connect", "submit", "spawn_connect", "initialize", "execute"}
    );

  //int i = 0;
  //for(rdmalib::Buffer<char> & buf : out) {
  //  printf("%d ", i++);
  //  for(int i = 0; i < std::min(10, opts.input_size); ++i)
  //    printf("%d ", ((char*)buf.data())[i]);
  //  printf("\n");
  //}

  instance.disconnect();

  return 0;
}
