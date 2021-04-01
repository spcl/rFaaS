
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>

#include <rfaas/executor.hpp>

#include "warm_benchmark.hpp"

//bool poll_result(client::ServerConnection & client, int iter)
//{
//  auto wc = client.recv().poll(true);
//  uint32_t val = ntohl(std::get<0>(wc)[0].imm_data) >> 6;
//  if(val == 0)
//    return true;
//  else {
//    if(val == 1)
//      spdlog::error("Iter {}, Thread busy, cannot post work", iter);
//    else
//      spdlog::error("Iter {}, Unknown error {}", iter, val);
//    return false;
//  }
//}

int main(int argc, char ** argv)
{
  auto opts = warm_benchmarker::options(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test warm_benchmarker!");

  rfaas::executor executor(opts.address, opts.port, opts.recv_buf_size, opts.max_inline_data);
  executor.allocate(opts.flib, opts.numcores, opts.input_size, -1, true);

  // FIXME: move me to allocator
  rdmalib::Buffer<char> in(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE), out(opts.input_size);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  memset(in.data(), 0, opts.input_size);
  for(int i = 0; i < opts.input_size; ++i) {
    ((char*)in.data())[i] = 1;
  }

  rdmalib::Benchmarker<1> benchmarker{opts.repetitions};
  spdlog::info("Warmups begin");
  for(int i = 0; i < opts.warmup_iters; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fname, in, out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for(int i = 0; i < opts.repetitions;) {
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
  spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}", opts.repetitions, avg, median);
  benchmarker.export_csv(opts.out_file, {"time"});

  for(int i = 0; i < std::min(100, opts.input_size); ++i)
    printf("%d ", ((char*)out.data())[i]);
  printf("\n");

  return 0;
  //std::ifstream in(opts["file"].as<std::string>());
  //client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in), recv_buf_size, max_inline_data);
  //in.close();
  //if(!client.connect())
  //  return -1;

  //client.allocate_send_buffers(2, buf_size);
  //client.allocate_receive_buffers(2, buf_size);
  //spdlog::info("Connected to the executor manager!");

  //// prepare args
  //memset(client.send_buffer(0).data(), 0, buf_size);
  //memset(client.send_buffer(1).data(), 0, buf_size);
  //for(int i = 0; i < buf_size; ++i) {
  //  ((char*)client.send_buffer(0).data())[i] = 1;
  //  ((char*)client.send_buffer(1).data())[i] = 2;
  //}

  //// Warmup iterations
  //rdmalib::Benchmarker<2> benchmarker{repetitions};
  ////rdmalib::RecvBuffer rcv_buffer{recv_buf_size};
  ////rcv_buffer.connect(&client.connection());
  //spdlog::info("Warmups begin");
  //for(int i = 0; i < warmup_iters; ++i) {
  //  client.recv().refill();
  //  SPDLOG_DEBUG("Submit warm {}", i);
  //  client.submit_fast(1, "test");
  //  poll_result(client, i);
  //}
  //spdlog::info("Warmups completed");

  //int pause = opts["pause"].as<int>();
  //std::vector<int> refills;
  //// Start actual measurements
  //for(int i = 0; i < repetitions;) {
  //  int b = client.recv().refill();
  //  benchmarker.start();
  //  int id = client.submit_fast(1, "test");
  //  SPDLOG_DEBUG("Submit actual {}", i);
  //  if(poll_result(client, i)) {
  //    benchmarker.end(0);
  //    SPDLOG_DEBUG("Finished execution");
  //    ++i;
  //  } else {
  //    continue;
  //  }
  //  if (b)
  //    refills.push_back(i);

  //  // Wait for the next iteration
  //  if(pause)
  //    std::this_thread::sleep_for(std::chrono::microseconds(pause));
  //}
  //auto [median, avg] = benchmarker.summary();
  //spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}", repetitions, avg, median);
  //std::cout << "Receive buffer refills; ";
  //for(int i = 0; i < refills.size(); ++i)
  //  std::cout << refills[i] << " ";
  //std::cout << '\n';

  //for(int i = 0; i < std::min(100, buf_size); ++i)
  //  printf("%d ", ((char*)client.recv_buffer(1).data())[i]);
  //printf("\n");

  return 0;
}
