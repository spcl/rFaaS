
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
  spdlog::info("Executing serverless-rdma test C++ interface.!");

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
  unsigned int rma_memory = 1024;
  rmafunctions::RmaFunctionConfig rma_config {"0", executor._device.port+100, rma_memory};
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

  spdlog::info("Non-Blocking execution, pause {}, size {}, write? {}", opts.pause, opts.read_size, opts.rdma_type);
  auto f = executor.async(opts.fname, in, out);
  // spdlog::info("NonBlocking execution done {}", f.get());

  rdmalib::RDMAActive active(rma_config.client_ip_address, rma_config.client_port, 32, 0);
  active.allocate();
  // TODO: connection management
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  if(!active.connect())
    return 1;

  // Initialize buffers for access to remote memory
  int buf_size = opts.read_size;
  rdmalib::Buffer<char> input(buf_size);
  rdmalib::Buffer<char> input2(buf_size);
  for(int i = 0; i < buf_size; ++i) {
    input.data()[i] = 'i';
    input2.data()[i] = 'o';
  }

  // Get memory address of remote memory buffer
  rdmalib::Buffer<char> data(12);
  data.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  input.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  input2.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  active.connection().post_recv(data);
  active.connection().poll_wc(rdmalib::QueueType::RECV, true, 1);
  auto r_address = *reinterpret_cast<uint64_t*>(data.data());
  auto r_key = *reinterpret_cast<uint32_t*>(data.data()+8);


  std::ofstream of("output", std::ios::out);
  while (true) {

    if (opts.rdma_type) {
      active.connection().post_write(
        input.sge(buf_size, 0),
        {r_address, r_key},
        false
      );
      active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
      spdlog::info("Post write {}", (input.data()[0]));
    }
    else {

      auto start = std::chrono::high_resolution_clock::now();
      active.connection().post_read(
        input2.sge(buf_size, 0),
        {r_address, r_key}
      );
      active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
      spdlog::info("Post read {}", (input2.data()[0]));

      auto end = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

      of << elapsed.count() << '\n';
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(opts.pause));

    active.connection().post_read(
      input2.sge(buf_size, 0),
      {r_address, r_key}
    );
    active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
    spdlog::info("RMA data read: {}", (input.data()[0]));
  }

  active.connection().close();
  f.get();
  executor.deallocate();
  instance.disconnect();

  spdlog::info("Finished!");
  return 0;
}
