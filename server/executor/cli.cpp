
#include <chrono>
#include <stdexcept>
#include <thread>
#include <climits>
#include <sys/time.h>

#include <signal.h>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include "rdmalib/connection.hpp"
#include "server.hpp"
#include "fast_executor.hpp"

int main(int argc, char ** argv)
{
  //server::SignalHandler sighandler;
  auto opts = server::opts(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info(
    "Executing serverless-rdma executor with {} cores! Waiting for client at {}:{}",
    opts.fast_executors, opts.address, opts.port
  );
  spdlog::info(
    "Configuration options: expecting function size {}, function payloads {},"
    " receive WCs buffer size {}, max inline data {}, hot polling timeout {}",
    opts.func_size, opts.msg_size, opts.recv_buffer_size, opts.max_inline_data,
    opts.timeout
  );
  spdlog::info(
    "My manager runs at {}:{}, its secret is {}, the accounting buffer is at {} with rkey {}",
    opts.mgr_address, opts.mgr_port, opts.mgr_secret,
    opts.accounting_buffer_addr, opts.accounting_buffer_rkey
  );

  executor::ManagerConnection mgr{
    opts.mgr_address,
    opts.mgr_port,
    opts.mgr_secret,
    opts.accounting_buffer_addr,
    opts.accounting_buffer_rkey
  };
  server::FastExecutors executor(
    opts.address, opts.port,
    opts.func_size,
    opts.fast_executors,
    opts.msg_size,
    opts.recv_buffer_size,
    opts.max_inline_data,
    opts.pin_threads,
    mgr
  );

  executor.allocate_threads(opts.timeout, opts.repetitions + opts.warmup_iters);

  executor.close();
  return 0;
}
