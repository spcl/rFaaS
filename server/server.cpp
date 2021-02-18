
#include <chrono>
#include <thread>
#include <climits>

#include <signal.h>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include "rdmalib/connection.hpp"
#include "server.hpp"


int main(int argc, char ** argv)
{
  server::SignalHandler sighandler;
  auto opts = server::opts(argc, argv);
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
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

  constexpr int cores_mask = 0x3F;
  while(!server::SignalHandler::closing) {
    // if we block, we never handle the interruption
    std::optional<ibv_wc> wc = conn->poll_wc(rdmalib::QueueType::RECV, false);
    bool correct_allocation = true;
    if(wc) {

      if(wc->status) {
        spdlog::error("Failed work completion! Reason: {}", ibv_wc_status_str(wc->status));
        continue;
      }
      int info = ntohl(wc->imm_data);
      int func_id = info >> 6;
      int core = info & cores_mask;
      SPDLOG_DEBUG("Execute func {} at core {} ptr {}", func_id, core, server._db.functions[func_id]);
      uint32_t cur_invoc = server._exec.get_invocation_id();
      server::InvocationStatus & invoc = server._exec.invocation_status(cur_invoc);
      invoc.connection = &*conn;
      server._exec.enable(core,
        {
          server._db.functions[func_id],
          &server._rcv[core],
          &server._send[core],
          cur_invoc
        }
      );
      server._exec.wakeup();
      int req_id = wc->wr_id;
      server.reload_queue(*conn, req_id);

      // Reenable: code for cheap invocation
      // Insert new rcv reequest
      //server.reload_queue(*conn, req_id);
      //rdmalib::functions::Submission * ptr = reinterpret_cast<rdmalib::functions::Submission*>(
      //    server._queue[wc->wr_id].data()
      //);

      //// Verify that core allocation makes sense
      //if(ptr->core_begin >= ptr->core_end || ptr->core_begin < 0 || ptr->core_end > numcores) {
      //  spdlog::error("Incorrect core allocation from {} to {}!", ptr->core_begin, ptr->core_end);
      //  correct_allocation = false;
      //}
      //for(int i = ptr->core_begin; i < ptr->core_end; ++i)
      //  if(!(*server._threads_allocation.data() & (1 << i))) {
      //    spdlog::error(
      //        "Requested allocating core {}, but current allocator is {}!",
      //        i,
      //        *server._threads_allocation.data()
      //    );
      //    correct_allocation = false;
      //  }


      //if(correct_allocation) {
      //  uint32_t cur_invoc = server._exec.get_invocation_id();
      //  server::InvocationStatus & invoc = server._exec.invocation_status(cur_invoc);
      //  invoc.active_threads.store(ptr->core_end - ptr->core_begin);
      //  invoc.connection = &*conn;
      //  spdlog::info(
      //      "Accepted invocation of function of id {}, internal invocation idx {}",
      //      ptr->ID,
      //      cur_invoc
      //  );

      //  for(int i = ptr->core_begin; i != ptr->core_end; ++i)
      //    server._exec.enable(i,
      //      {
      //        server._db.functions[ptr->ID],
      //        &server._rcv[i],
      //        &server._send[i],
      //        cur_invoc
      //      }
      //    );
      //  server._exec.wakeup();
      //} else {
      //  // TODO: send a reply that something went wrong
      //}
    }
  }
  spdlog::info("Server is closing down");
  std::this_thread::sleep_for(std::chrono::seconds(1)); 

  return 0;
}
