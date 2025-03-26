
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
// TODO
#include "../examples/rma_functions.hpp"

#include <unistd.h>

int main(int argc, char ** argv)
{
  auto opts = rfaas::benchmark::options(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test rma!");

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

  // RMA function config
  int function_input_buffer_len = 1;
  int function_input_buffer_size = function_input_buffer_len*sizeof(rmafunctions::RmaFunctionConfig);
  unsigned int rma_memory = opts.rma_memory;
  rmafunctions::RmaFunctionConfig rma_config {"0", 54328, rma_memory};
  strncpy(rma_config.client_ip_address, executor._device.ip_address.c_str(), rmafunctions::IPV4_ADDRESS_STRING_LENGTH);

  if (!executor.allocate(opts.flib, function_input_buffer_size,
                         settings.benchmark.hot_timeout, false, skip_resource_manager)) {
    spdlog::error("Connection to executor and allocation failed!");
    return 1;
  }

  // Initialize input buffers and send rma_config
  rdmalib::Buffer<rmafunctions::RmaFunctionConfig> in(function_input_buffer_len, rdmalib::functions::Submission::DATA_HEADER_SIZE), out(function_input_buffer_len);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  in.data()[0] = rma_config;

  spdlog::info("Invoke RMA function");
  auto f = executor.async(opts.fname, in, out);
  // spdlog::info("NonBlocking execution done {}", f.get());

  spdlog::info("Connecting to RMA function...");
  rdmalib::RDMAActive active;
  while (true) {
    rdmalib::RDMAActive tmp_active(rma_config.client_ip_address, rma_config.client_port, 32, 0);
    if (tmp_active.connect()) {
      active = std::move(tmp_active);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  spdlog::info("Connected to RMA function");

  // Initialize buffers for access to remote memory
  int buf_size = opts.input_size;
  rdmalib::Buffer<char> input(buf_size);
  for(int i = 0; i < buf_size; ++i) {
    input.data()[i] = 'i';
  }

  // Get memory address of remote memory buffer
  rdmalib::Buffer<char> data(12);
  data.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  input.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  active.connection().post_recv(data);
  active.connection().poll_wc(rdmalib::QueueType::RECV, true, 1);
  auto r_address = *reinterpret_cast<uint64_t*>(data.data());
  auto r_key = *reinterpret_cast<uint32_t*>(data.data()+8);


  spdlog::info("Starting benchmark with: pause {} ms, payload size {}, write? {}", opts.pause, opts.input_size, opts.rma_mode);

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  int warmup_count = settings.benchmark.warmup_repetitions;
  int iteration_count = settings.benchmark.repetitions;
  if (warmup_count > 0)
    spdlog::info("Warmups begin");

  while (iteration_count > 0) {
    
    if (warmup_count <= 0)
      benchmarker.start();

    if (opts.rma_mode) {
      active.connection().post_write(
        input.sge(buf_size, 0),
        {r_address, r_key},
        false
      );
      spdlog::debug("Posted write {}", (input.data()[0]));
    }
    else {
      active.connection().post_read(
        input.sge(buf_size, 0),
        {r_address, r_key}
      );
      spdlog::debug("Posted read {}", (input.data()[0]));
    }

    auto [wc, ret] = active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
    if (wc[0].status != 0) {
      spdlog::error("Error when posting read/write to rma function");
      return 1;
    }

    if (warmup_count <= 0)
      benchmarker.end(0);
    
    if (warmup_count > 0) {
      warmup_count--;
      if (warmup_count == 0)
        spdlog::info("Warmups completed");
    }
    else {
      iteration_count--;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(opts.pause));
  }

  auto [median, avg] = benchmarker.summary();
  spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}", settings.benchmark.repetitions, avg, median);
  if (opts.output_stats != "")
    benchmarker.export_csv(opts.output_stats, {"time"});

  active.connection().close();
  f.get();
  executor.deallocate();
  instance.disconnect();

  spdlog::info("Finished!");
  return 0;
}
