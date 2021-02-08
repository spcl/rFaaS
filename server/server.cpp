
#include <chrono>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <rdma/rdma_verbs.h>
#include <infiniband/verbs.h>

#include <rdmalib.hpp>


int main(int argc, char ** argv)
{

  cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
  options.add_options()
    ("a,address", "Use selected address", cxxopts::value<std::string>())
    ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
  ;
  auto opts = options.parse(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::info("Executing serverless-rdma server!");

  // Start RDMA connection
  rdmalib::Buffer<char> mr(4096);
  rdmalib::RDMAPassive state(opts["address"].as<std::string>(), opts["port"].as<int>());
  state.allocate();
  mr.register_memory(state.pd(), IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
  auto conn = state.poll_events();
  if(!conn)
    return -1;
 
  memset(mr.data(), 6, 4096);
  state.post_send(*conn, mr);
  auto wc = state.poll_wc(*conn, rdmalib::QueueType::SEND);
  spdlog::info("Message sent {} {}", ibv_wc_status_str(wc.status), wc.wr_id);
  
  memset(mr.data(), 7, 4096);
  do {
    state.post_send(*conn, mr);
    wc = state.poll_wc(*conn, rdmalib::QueueType::SEND);
    spdlog::info("Message sent {} {}, retry", ibv_wc_status_str(wc.status), wc.wr_id);
  } while(wc.status != 0);

  // immediate
  state.post_recv(*conn, {});
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

  while(1) {
    //auto wc = state.poll_wc(*conn);
    struct ibv_wc wc;
    int ret = ibv_poll_cq(conn->qp->recv_cq, 1, &wc);
    if(ret != 0) 
      spdlog::info("WC status {} {}", ibv_wc_status_str(wc.status), ntohl(wc.imm_data));
    else
      spdlog::info("No events");
    for(int i = 0; i < 100; ++i)
      printf("%d ", mr.data()[i]);
    printf("\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
   }
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
