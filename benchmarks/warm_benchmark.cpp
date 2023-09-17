
#include <chrono>
#include <thread>
#include <fstream>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>

#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>
#include <rfaas/rdma_allocator.hpp>

#include "warm_benchmark.hpp"
#include "settings.hpp"

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

  // Sample: test demonstrating standard memory allocation.

  // rdmalib::Buffer<char> in(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE), out(opts.input_size);
  // in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  // out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);


  // Sample: test demonstrating allocation with our custom allocator.

  //  rfaas::RdmaInfo info_in(executor,IBV_ACCESS_LOCAL_WRITE,rdmalib::functions::Submission::DATA_HEADER_SIZE);
  //  rfaas::RdmaAllocator<rdmalib::Buffer<char>> allocator_in{info_in};
  //  rdmalib::Buffer<char>* in0 = allocator_in.allocate(opts.input_size);
  //  allocator_in.construct(in0, opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
  //
  //  rfaas::RdmaInfo info_out(executor,(IBV_ACCESS_LOCAL_WRITE| IBV_ACCESS_REMOTE_WRITE));
  //  rfaas::RdmaAllocator<rdmalib::Buffer<char>> allocator_out{info_out};
  //  rdmalib::Buffer<char>* out0 = allocator_out.allocate(opts.input_size);
  //  allocator_out.construct(out0, opts.input_size);


  // Sample: test demonstrating allocation with std::vector.

  rfaas::RdmaInfo info_v_in(executor, IBV_ACCESS_LOCAL_WRITE, rdmalib::functions::Submission::DATA_HEADER_SIZE);
  rfaas::RdmaAllocator<rdmalib::Buffer<char>> allocator_v_in{info_v_in};
  std::vector<rdmalib::Buffer<char>, rfaas::RdmaAllocator<rdmalib::Buffer<char>>> v_in(allocator_v_in);

  rfaas::RdmaInfo info_v_out(executor, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
  rfaas::RdmaAllocator<rdmalib::Buffer<char>> allocator_v_out{info_v_out};
  std::vector<rdmalib::Buffer<char>, rfaas::RdmaAllocator<rdmalib::Buffer<char>>> v_out(allocator_v_out);

  v_in.emplace_back(static_cast<size_t>(opts.input_size), rdmalib::functions::Submission::DATA_HEADER_SIZE);
  v_out.emplace_back(static_cast<size_t>(opts.input_size));

  rdmalib::Buffer<char> *in = &v_in[0];
  rdmalib::Buffer<char> *out = &v_out[0];


  // TODO: Since the for loop writes a value of 1 to each byte of the in buffer,
  //       it overwrites all bytes previously set to 0 by the memset() function.
  memset(in->data(), 0, opts.input_size);
  for (int i = 0; i < opts.input_size; ++i) {
    ((char *) in->data())[i] = 1;
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Warmups begin");
  for(int i = 0; i < settings.benchmark.warmup_repetitions; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fname, *in, *out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for(int i = 0; i < settings.benchmark.repetitions;) {
    benchmarker.start();
    SPDLOG_DEBUG("Submit execution {}", i);
    auto ret = executor.execute(opts.fname, *in, *out);
    if(std::get<0>(ret)) {
      SPDLOG_DEBUG("Finished execution {} out of {}", i, settings.benchmark.repetitions);
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

  printf("Data: ");
  for(int i = 0; i < std::min(100, opts.input_size); ++i)
    printf("%d ", ((char*)out->data())[i]);
  printf("\n");

//  std::free(&v_in);
//  std::free(&v_out);
//  v_in.get_allocator().deallocate(&v_in[0],opts.input_size);
//  v_out.get_allocator().deallocate(&v_out[0],opts.input_size);
  return 0;
}
