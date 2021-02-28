
#include <chrono>
#include <thread>
#include <sys/time.h>

#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include "client.hpp"


int main(int argc, char ** argv)
{
  auto opts = client::options(argc, argv);
  int buf_size = opts["size"].as<int>();
  int repetitions = opts["repetitions"].as<int>();
  int warmup_iters = opts["warmup-iters"].as<int>();
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma client!");

  std::ifstream in(opts["file"].as<std::string>());
  client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in), buf_size);
  in.close();
  client.allocate_send_buffers(2, buf_size);
  client.allocate_receive_buffers(2, buf_size);
  if(!client.connect())
    return -1;
  spdlog::info("Connected to the server!");

  // prepare args
  memset(client.send_buffer(0).data(), 0, buf_size);
  memset(client.send_buffer(1).data(), 0, buf_size);
  for(int i = 0; i < buf_size; ++i) {
    ((int*)client.send_buffer(0).data())[i] = 1;
    ((int*)client.send_buffer(1).data())[i] = 2;
  }

  // Warmup iterations
  int buffer_refill = std::min(buf_size, 5);
  timeval start, end;
  int requests = buf_size;
  int sum = 0;
  client.connection().post_recv({}, -1, buf_size);
  spdlog::info("Warmups begin");
  SPDLOG_DEBUG("Warmups begin");
  for(int i = 0; i < warmup_iters; ++i) {
    if(requests < buffer_refill) {
      client.connection().post_recv({}, -1, buf_size - requests); requests = buf_size;
    }
    client.submit_fast(1, "test");
    auto wc = client.connection().poll_wc(rdmalib::QueueType::RECV);
    requests--;
  }
  spdlog::info("Warmups completed");
  SPDLOG_DEBUG("Warmups completed");

  // Start actual measurements
  gettimeofday(&start, nullptr);
  for(int i = 0; i < repetitions; ++i) {
    if(requests < buffer_refill) {
      client.connection().post_recv({}, -1, buf_size - requests); requests = buf_size;
    }
    int id = client.submit_fast(1, "test");
    auto wc = client.connection().poll_wc(rdmalib::QueueType::RECV);
    requests--;
    SPDLOG_DEBUG("Finished execution with ID {}", ntohl(wc->imm_data)); 
  }
  gettimeofday(&end, nullptr);
  int usec = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
  sum += usec;
  spdlog::info("Executed {} repetitions in {} usec, avg {} usec/iter", repetitions, sum, ((float)sum)/repetitions);

  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(0).data())[i]);
  printf("\n");
  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(1).data())[i]);
  printf("\n");

  return 0;
}
