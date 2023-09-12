
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
#include <rdmalib/connection.hpp>

#include "manager.hpp"

rfaas::executor_manager::Manager * instance = nullptr;

void signal_handler(int)
{
  assert(instance);
  instance->shutdown();
}

int main(int argc, char ** argv)
{
  int rc = ibv_fork_init();
  if(rc) {
    spdlog::error("ibv_fork_init failed, cannot continue! Error code {}", rc);
    exit(rc);
  }

  auto opts = rfaas::executor_manager::opts(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing rFaaS executor manager!");

  // Catch SIGINT
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = &signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Read device details
  std::ifstream in_dev{opts.device_database};
  if(!in_dev.is_open()) {
    spdlog::error(fmt::format("Couldn't open devices file: {}", opts.device_database));
    return 1;
  }
  rfaas::devices::deserialize(in_dev);

  // Read executor manager settings
  std::ifstream in_cfg{opts.json_config};
  if(!in_cfg.is_open()) {
    spdlog::error(fmt::format("Couldn't open executor manager config file: {}", opts.json_config));
    return 1;
  }
  rfaas::executor_manager::Settings settings = rfaas::executor_manager::Settings::deserialize(in_cfg);

  rfaas::executor_manager::Manager mgr{settings, opts.skip_rm};
  instance = &mgr;
  mgr.start();

  spdlog::info("Executor manager is closing down");
  std::this_thread::sleep_for(std::chrono::seconds(1)); 

  return 0;
}
