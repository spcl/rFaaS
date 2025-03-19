
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

#include <unistd.h>

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

  //spdlog::info("Blocking execution");
  //executor.execute(opts.fname, ins[0], outs[0]);
  //spdlog::info("Blocking execution done");

  spdlog::info("Non-Blocking execution, pause {}, size {}, write? {}", opts.pause, opts.read_size, opts.rdma_type);
  auto f = executor.async(opts.fname, ins[0], outs[0]);
  //spdlog::info("NonBlocking execution done {}", f.get());

  rdmalib::RecvBuffer _rcv_buffer{32};
  rdmalib::RDMAActive active(opts.rma_address, 20000, 32, 0);
  active.allocate();
  if(!active.connect())
    return 1;

  // receive buffer data
  int buf_size = opts.read_size;
  rdmalib::Buffer<char> input(buf_size);
  rdmalib::Buffer<char> input2(buf_size);
  for(int i = 0; i < buf_size; ++i) {
    input.data()[i] = 1;
    input2.data()[i] = 0;
  }

  rdmalib::Buffer<char> data(12);
  data.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  input.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  input2.register_memory(active.pd(), IBV_ACCESS_LOCAL_WRITE);
  active.connection().post_recv(data);
  active.connection().poll_wc(rdmalib::QueueType::RECV, true, 1);
  auto r_address = *reinterpret_cast<uint64_t*>(data.data());
  auto r_key = *reinterpret_cast<uint32_t*>(data.data()+8);

  _rcv_buffer.connect(&active.connection());

  int i = 0;
  std::ofstream of("output", std::ios::out);
  while(true) {
    //spdlog::info("Post write {}", static_cast<int>(input.data()[0]));

    if(opts.rdma_type) {
      active.connection().post_write(
        input.sge(buf_size, 0),
        {r_address, r_key},
        false
      );
      active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
      if(i % 10 == 0)
        spdlog::info("Posted write {}, sleep", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.pause));
    } else {
    //spdlog::info("Sleep");
    //sleep(1);
    //spdlog::info("Post read {}", static_cast<int>(input2.data()[0]));
      auto start = std::chrono::high_resolution_clock::now();
      active.connection().post_read(
        input2.sge(buf_size, 0),
        {r_address, r_key}
      );
      active.connection().poll_wc(rdmalib::QueueType::SEND, true, 1);
      auto end = std::chrono::high_resolution_clock::now();
 
      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

      of << elapsed.count() << '\n';
      if(i % 10 == 0)
        spdlog::info("Done read {}, sleep", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.pause));
    }
    ++i;
    //spdlog::info("Got read {}", static_cast<int>(input2.data()[0]));
    //for(int i = 0; i < buf_size; ++i)
    //input.data()[i]++;
  }
    //spdlog::info("Polled send");
    //_rcv_buffer.poll(true);

  active.connection().close();

  spdlog::info("Connected to RMA");
  f.get();
  spdlog::info("Finished!");

  //spdlog::info("Non-Blocking execution once more");
  //auto f2 = executor.async(opts.fname, ins[0], outs[0]);
  //spdlog::info("NonBlocking execution done {}", f2.get());

  // The result of future should arrive while polling for blocking result
  //spdlog::info("Mixed blocking and non-blocking execution");
  //memset(outs[0].data(), 0, opts.input_size);
  //memset(outs[1].data(), 0, opts.input_size);
  //auto f3 = executor.async(opts.fname, ins[0], outs[0]);
  //executor.execute(opts.fname, ins[1], outs[1]);
  //buf_idx = 1;
  //for(rdmalib::Buffer<char> & buf : outs) {
  //  printf("Output %d Data: ", buf_idx++);
  //  for(int i = 0; i < std::min(100, opts.input_size); ++i)
  //    printf("%d ", ((char*)buf.data())[i]);
  //  printf("\n");
  //}
  //spdlog::info("Mixed execution done {}", f3.get());

  //spdlog::info("Non-Blocking execution on 2 buffers");
  //auto f4 = executor.async(opts.fname, ins, outs);
  //int ret = f4.get();
  //buf_idx = 1;
  //for(rdmalib::Buffer<char> & buf : outs) {
  //  printf("Output %d Data: ", buf_idx++);
  //  for(int i = 0; i < std::min(100, opts.input_size); ++i)
  //    printf("%d ", ((char*)buf.data())[i]);
  //  printf("\n");
  //}
  //spdlog::info("NonBlocking execution done {}", ret);

  executor.deallocate();

  return 0;
}
