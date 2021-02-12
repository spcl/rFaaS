
#include <chrono>
#include <thread>
#include <climits>

#include <signal.h>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include "server.hpp"


int main(int argc, char ** argv)
{
  server::SignalHandler sighandler;
  auto opts = server::opts(argc, argv);
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

  while(!server::SignalHandler::closing) {
    //auto wc = server._state.poll_wc(*conn, rdmalib::QueueType::RECV);
    struct ibv_wc wc;
    int ret = ibv_poll_cq(conn->qp()->recv_cq, 1, &wc);
    if(ret != 0) {
      if(wc.status) {
        spdlog::warn("Failed work completion! Reason: {}", ibv_wc_status_str(wc.status));
        continue;
      }
      // TODO: add sanity check - cores numbers make sense
      server.reload_queue(*conn, wc.wr_id);
      rdmalib::functions::Submission * ptr = reinterpret_cast<rdmalib::functions::Submission*>(
          server._queue[wc.wr_id].data()
      );
      for(int i = ptr->core_begin; i < ptr->core_end; ++i)
        if(!(*server._threads_allocation.data() & (1 << i)))
          spdlog::error("Requested allocating core {}, but current allocator is {}!", i, *server._threads_allocation.data());
      int cur_invoc = server._exec.get_invocation_id();
      std::get<0>(server._exec._invocations[cur_invoc]) = 0;
      std::get<1>(server._exec._invocations[cur_invoc]).store(ptr->core_end - ptr->core_begin);
      std::get<2>(server._exec._invocations[cur_invoc]) = &*conn;
      // TODO: send errror if cores don't match
      spdlog::info("Accepted invocation of function of id {}, internal invocation idx {}", ptr->ID, cur_invoc);

      for(int i = ptr->core_begin; i != ptr->core_end; ++i)
        server._exec.enable(i, std::make_tuple(
          server._db.functions[ptr->ID],
          &server._rcv[i],
          &server._send[i],
          cur_invoc
        ));
      server._exec.wakeup();
    }
  }
  spdlog::info("Server is closing down");
  std::this_thread::sleep_for(std::chrono::seconds(1)); 

  return 0;
}
