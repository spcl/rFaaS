
#include <chrono>
#include <thread>
#include <string>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/allocation.hpp>
#include <rdmalib/functions.hpp>

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

  // Read connection details to the executors
  if(opts.executors_database != "") {
    std::ifstream in_cfg(opts.executors_database);
    rfaas::servers::deserialize(in_cfg);
    in_cfg.close();
  } else {
    spdlog::error(
      "Connection to resource manager is temporarily disabled, use executor database "
      "option instead!"
    );
    return 1;
  }

  rfaas::executor executor(
    settings.device->ip_address,
    settings.rdma_device_port,
    settings.device->default_receive_buffer_size,
    settings.device->max_inline_data
  );
  std::vector<rdmalib::Buffer<char>> in;
  std::vector<rdmalib::Buffer<char>> out;
  for(int i = 0; i < opts.cores; ++i) {
    in.emplace_back(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
    in.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
    memset(in.back().data(), 0, opts.input_size);
    for(int i = 0; i < opts.input_size; ++i) {
      ((char*)in.back().data())[i] = 1;
    }
  }
  for(int i = 0; i < opts.cores; ++i) {
    out.emplace_back(opts.input_size);
    out.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    memset(out.back().data(), 0, opts.input_size);
  }

  rdmalib::Benchmarker<5> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Measurements begin");
  auto start = std::chrono::high_resolution_clock::now();
  for(int i = 0; i < settings.benchmark.repetitions;++i) {
    spdlog::info("Begin iteration {}", i);
    if(executor.allocate(
      opts.flib, opts.cores, opts.input_size, 
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

  int i = 0;
  for(rdmalib::Buffer<char> & buf : out) {
    printf("%d ", i++);
    for(int i = 0; i < std::min(10, opts.input_size); ++i)
      printf("%d ", ((char*)buf.data())[i]);
    printf("\n");
  }

  return 0;
}
