
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

#include "cpp_interface.hpp"
#include "settings.hpp"

int main(int argc, char ** argv)
{
  auto opts = cpp_interface::options(argc, argv);
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
    2,
    opts.input_size,
    settings.benchmark.hot_timeout,
    false
  )) {
    spdlog::error("Connection to executor and allocation failed!");
    return 1;
  }
  rdmalib::Buffer<char> in(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE), out(opts.input_size);
  rdmalib::Buffer<char> in2(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE), out2(opts.input_size);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  in2.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out2.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  std::vector<rdmalib::Buffer<char>> ins;
  ins.push_back(std::move(in));
  ins.push_back(std::move(in2));
  std::vector<rdmalib::Buffer<char>> outs;
  outs.push_back(std::move(out));
  outs.push_back(std::move(out2));
  // Iniitalize input buffers
  int buf_idx = 1;
  for(rdmalib::Buffer<char> & buf : ins) {
    for(int i = 0; i < opts.input_size; ++i) {
      ((char*)buf.data())[i] = buf_idx;
    }
    buf_idx++;
  }
  for(rdmalib::Buffer<char> & buf : outs) {
    memset(buf.data(), 0, opts.input_size);
  }
  buf_idx = 0;
  for(rdmalib::Buffer<char> & buf : ins) {
    printf("Input %d Data: ", buf_idx++);
    for(int i = 0; i < std::min(100, opts.input_size); ++i)
      printf("%d ", ((char*)buf.data())[i]);
    printf("\n");
  }

  spdlog::info("Blocking execution");
  executor.execute(opts.fname, ins[0], outs[0]);
  spdlog::info("Blocking execution done");

  spdlog::info("Non-Blocking execution");
  auto f = executor.async(opts.fname, ins[0], outs[0]);
  spdlog::info("NonBlocking execution done {}", f.get());

  spdlog::info("Non-Blocking execution once more");
  auto f2 = executor.async(opts.fname, ins[0], outs[0]);
  spdlog::info("NonBlocking execution done {}", f2.get());

  // The result of future should arrive while polling for blocking result
  spdlog::info("Mixed blocking and non-blocking execution");
  memset(outs[0].data(), 0, opts.input_size);
  memset(outs[1].data(), 0, opts.input_size);
  auto f3 = executor.async(opts.fname, ins[0], outs[0]);
  executor.execute(opts.fname, ins[1], outs[1]);
  buf_idx = 1;
  for(rdmalib::Buffer<char> & buf : outs) {
    printf("Output %d Data: ", buf_idx++);
    for(int i = 0; i < std::min(100, opts.input_size); ++i)
      printf("%d ", ((char*)buf.data())[i]);
    printf("\n");
  }
  spdlog::info("Mixed execution done {}", f3.get());

  spdlog::info("Non-Blocking execution on 2 buffers");
  auto f4 = executor.async(opts.fname, ins, outs);
  int ret = f4.get();
  buf_idx = 1;
  for(rdmalib::Buffer<char> & buf : outs) {
    printf("Output %d Data: ", buf_idx++);
    for(int i = 0; i < std::min(100, opts.input_size); ++i)
      printf("%d ", ((char*)buf.data())[i]);
    printf("\n");
  }
  spdlog::info("NonBlocking execution done {}", ret);

  executor.deallocate();

  return 0;
}
