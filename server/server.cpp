
#include <chrono>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include "server.hpp"

int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
  options.add_options()
    ("a,address", "Use selected address", cxxopts::value<std::string>())
    ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
    ("n,numcores", "Number of cores", cxxopts::value<int>()->default_value("1"))
    ("f,file", "Output server status.", cxxopts::value<std::string>())
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma server!");

  // Start RDMA connection
  int numcores = opts["numcores"].as<int>();
  server::Server server(
      opts["address"].as<std::string>(),
      opts["port"].as<int>(),
      numcores
  );
  server.allocate_send_buffers(numcores, 4096);
  server.allocate_rcv_buffers(numcores, 4096);
  //rdmalib::Buffer<char> mr(4096);
  //rdmalib::RDMAPassive state(opts["address"].as<std::string>(), opts["port"].as<int>());
  //state.allocate();
  //mr.register_memory(state.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
  //rdmalib::server::ServerStatus status(opts["address"].as<std::string>(), opts["port"].as<int>());
  //status.add_buffer(mr);
  {
    std::ofstream out(opts["file"].as<std::string>());
    server.status().serialize(out);
  }

  // Wait for a client
  auto conn = server.poll_communication();
  if(!conn)
    return -1;
  // TODO: Display client's address
  spdlog::info("Connected a client!");

  while(1) {
    //auto wc = state.poll_wc(*conn);
    //struct ibv_wc wc;
    //int ret = ibv_poll_cq(conn->qp->recv_cq, 1, &wc);
    //if(ret != 0) {
    //  spdlog::info("WC status {} {}", ibv_wc_status_str(wc.status), ntohl(wc.imm_data));
    //  buffer = ntohl(wc.imm_data);
    //  exec.enable(0, server.db.functions["test"], &buffer);
    //  exec.enable(1, server.db.functions["test"], &buffer);
    //  exec.wakeup();
    //} else
    //  spdlog::info("No events");
    //for(int i = 0; i < 100; ++i)
    //  printf("%d ", mr.data()[i]);
    //printf("\n");
    //std::this_thread::sleep_for(std::chrono::seconds(1));
  }
 
  //memset(mr.data(), 6, 4096);
  //state.post_send(*conn, mr);
  //auto wc = state.poll_wc(*conn, rdmalib::QueueType::SEND);
  //spdlog::info("Message sent {} {}", ibv_wc_status_str(wc.status), wc.wr_id);
  //
  //memset(mr.data(), 7, 4096);
  //do {
  //  state.post_send(*conn, mr);
  //  wc = state.poll_wc(*conn, rdmalib::QueueType::SEND);
  //  spdlog::info("Message sent {} {}, retry", ibv_wc_status_str(wc.status), wc.wr_id);
  //} while(wc.status != 0);


  //server::Server server;
  //server::Executors exec(2);
  //// immediate
  //state.post_recv(*conn, {});
  //state.poll_wc(*conn, rdmalib::QueueType::RECV);
  //ibv_sge sge;
  //memset(&sge, 0, sizeof(sge));
  //struct ibv_recv_wr wr, *bad;
  //wr.wr_id = 30;
  //wr.next = nullptr;
  //wr.sg_list = nullptr;
  //wr.num_sge = 0;

  //int ret = ibv_post_recv(conn->qp, &wr, &bad);
  //if(ret) {
  //  spdlog::error("Post receive unsuccesful, reason {} {}", errno, strerror(errno));
  //  return -1;
  //}
  //int buffer = 0;
  //while(1) {
  //  //auto wc = state.poll_wc(*conn);
  //  struct ibv_wc wc;
  //  int ret = ibv_poll_cq(conn->qp->recv_cq, 1, &wc);
  //  if(ret != 0) {
  //    spdlog::info("WC status {} {}", ibv_wc_status_str(wc.status), ntohl(wc.imm_data));
  //    buffer = ntohl(wc.imm_data);
  //    exec.enable(0, server.db.functions["test"], &buffer);
  //    exec.enable(1, server.db.functions["test"], &buffer);
  //    exec.wakeup();
  //  } else
  //    spdlog::info("No events");
  //  for(int i = 0; i < 100; ++i)
  //    printf("%d ", mr.data()[i]);
  //  printf("\n");
  //  std::this_thread::sleep_for(std::chrono::seconds(1));
  // }
  //// Start measurement
  //std::this_thread::sleep_for(std::chrono::seconds(1));
  //  for(int i = 0; i < 100; ++i)
  //    printf("%d ", mr.data()[i]);
  //  printf("\n");

  // Write request

  // Wait for reading results

  // Stop measurement

  //std::this_thread::sleep_for(std::chrono::seconds(5));
  //spdlog::info("Finalizing");
  //for(int i = 0; i < 100; ++i)
  //  printf("%d ", mr.data()[i]);
  //printf("\n");

  return 0;
}
