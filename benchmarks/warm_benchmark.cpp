
#include <atomic>
#include <chrono>
#include <thread>
#include <fstream>

#include <rdma/fabric.h>
#include <signal.h>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>

#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

#include "warm_benchmark.hpp"
#include "settings.hpp"

std::atomic<bool> finish_work;

void signal_handler(int)
{
  finish_work.store(true, std::memory_order_release);
  //std::terminate();
}

int main(int argc, char ** argv)
{
  auto opts = warm_benchmarker::options(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test warm_benchmarker!");

  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);
  in_dev.close();

  rdmalib::Configuration::get_instance().configure_cookie(
    rfaas::devices::instance()._configuration.authentication_credential
  );

  // Read benchmark settings
  std::ifstream benchmark_cfg{opts.json_config};
  rfaas::benchmark::Settings settings = rfaas::benchmark::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  spdlog::info("Execute {} iterations of function.", settings.benchmark.repetitions);

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
  if(!executor.allocate(
    opts.flib,
    1,
    opts.input_size,
    settings.benchmark.hot_timeout,
    false
  )) {
    spdlog::error("Connection to executor and allocation failed!");
    return 1;
  }

  // FIXME: move me to a memory allocator
  rdmalib::Buffer<char> in(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE), out(opts.input_size);
  #ifdef USE_LIBFABRIC
  in.register_memory(executor._state.pd(), FI_WRITE);
  out.register_memory(executor._state.pd(), FI_WRITE | FI_REMOTE_WRITE);
  #else
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  #endif
  memset(in.data(), 0, opts.input_size);
  for(int i = 0; i < opts.input_size; ++i) {
    ((char*)in.data())[i] = 1;
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};

  if(settings.benchmark.repetitions > 0) {

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
      auto ret = executor.execute(opts.fname, in, out);
      if(std::get<0>(ret)) {
        SPDLOG_DEBUG("Finished execution {} out of {}", i, settings.benchmark.repetitions);
        benchmarker.end(0);
        ++i;
      } else {
        continue;
      }
    }
  }
  // Infinite loop repetitions 
  else {

    finish_work.store(false, std::memory_order_release);

    // Catch SIGINT
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = &signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

    // Start actual measurements
    int i = 0;
    while(true) {
      
      bool flag = finish_work.load(std::memory_order_acquire);
      if(flag) {
        spdlog::info("Flag {}", flag);
        break;
      } else
        spdlog::info("Flag {}", flag);
      benchmarker.start();
      SPDLOG_DEBUG("Submit execution {}", i);
      auto ret = executor.execute(opts.fname, in, out);
      if(std::get<0>(ret)) {
        SPDLOG_DEBUG("Finished execution {}", i);
        benchmarker.end(0);
        ++i;
      } else {
        continue;
      }
    }
    spdlog::info("Caught SIGINT, ending benchmarking");
    std::flush(std::cout);

  }

  auto [median, avg] = benchmarker.summary();
  spdlog::info(
    "Executed {} repetitions, avg {} usec/iter, median {}",
    settings.benchmark.repetitions, avg, median
  );
  std::flush(std::cout);
  if(opts.output_stats != "")
    benchmarker.export_csv(opts.output_stats, {"time"});
  executor.deallocate();

  printf("Data: ");
  for(int i = 0; i < std::min(100, opts.input_size); ++i)
    printf("%d ", ((char*)out.data())[i]);
  printf("\n");

  return 0;
}
