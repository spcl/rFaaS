
#include <chrono>
#include <thread>
#include <sys/time.h>

#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include "client.hpp"


int main(int argc, char ** argv)
{
  auto opts = client::options(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma client!");

  std::ifstream in(opts["file"].as<std::string>());
  client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in));
  in.close();
  client.allocate_send_buffers(2, 4096);
  client.allocate_receive_buffers(2, 4096);
  if(!client.connect())
    return -1;
  spdlog::info("Connected to the server!");
  //std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // prepare args
  memset(client.send_buffer(0).data(), 0, 4096);
  memset(client.send_buffer(1).data(), 0, 4096);
  for(int i = 0; i < 100; ++i) {
    ((int*)client.send_buffer(0).data())[i] = 1;
    ((int*)client.send_buffer(1).data())[i] = 2;
  }

  //client._submit_buffer.data()[0] = 100;
  //client._active.post_send(client._submit_buffer);
  //client._active.poll_wc(rdmalib::QueueType::SEND);
  //
  int sum = 0;
  timeval start, end;
  int repetitions = opts["repetitions"].as<int>();
  client.submit_fast(1, "test");
  auto wc = client.connection().poll_wc(rdmalib::QueueType::RECV);
  for(int i = 0; i < repetitions; ++i) {

    gettimeofday(&start, nullptr); 
    client.submit_fast(1, "test");
    auto wc = client.connection().poll_wc(rdmalib::QueueType::RECV);
    gettimeofday(&end, nullptr);
    SPDLOG_DEBUG("Finished execution with ID {}", ntohl(wc->imm_data)); 
    //spdlog::flush_on(spdlog::level::info);
    int usec = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    sum += usec;
  }
  spdlog::info("Executed {} repetitions in {} usec, avg {} usec/iter", repetitions, sum, ((float)sum)/repetitions);
  //client._active.poll_wc(rdmalib::QueueType::SEND);
  //std::this_thread::sleep_for(std::chrono::seconds(1));
  // results should be here
  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(0).data())[i]);
  printf("\n");
  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(1).data())[i]);
  printf("\n");

  return 0;
}
