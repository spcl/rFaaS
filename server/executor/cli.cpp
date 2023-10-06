
#include <chrono>
#include <set>
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

std::vector<int> parse_thread_pinning(const std::string& input)
{
  std::vector<int> cores;
  const std::string delimiter = ";";
  std::string::size_type pos_begin = 0, pos_end = 0;

  while((pos_begin = input.find_first_not_of(delimiter, pos_end)) != std::string::npos) {
    pos_end = input.find_first_of(delimiter, pos_begin);
    if(pos_end == std::string::npos) {
      pos_end = input.length();
    }

    cores.push_back( std::stoi(input.substr(pos_begin, pos_end - pos_begin)));
  }

  return cores;
}

int main(int argc, char ** argv)
{
  // Register a SIGINT handler so that we can gracefully exit
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
    "My manager runs at {}:{}, its secret is {}, the accounting buffer is at {} with rkey {}, cookie {}",
    opts.mgr_address, opts.mgr_port, opts.mgr_secret,
    opts.accounting_buffer_addr, opts.accounting_buffer_rkey
    #ifdef USE_GNI_AUTH
    ,opts.authentication_cookie
    #endif
  );

  #ifdef USE_GNI_AUTH
  rdmalib::Configuration::get_instance().configure_cookie(
    opts.authentication_cookie
  );
  #endif

  // Parse thread pinning options
  std::vector<int> pin_threads = parse_thread_pinning(opts.pin_threads);

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
    pin_threads,
    mgr
  );

  executor.allocate_threads(opts.timeout, opts.repetitions + opts.warmup_iters);

  executor.close();
  return 0;
}
