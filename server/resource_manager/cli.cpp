
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

#include <rfaas/devices.hpp>

#include "manager.hpp"

rfaas::resource_manager::Manager * instance = nullptr;

void signal_handler(int)
{
  assert(instance);
  spdlog::debug("kill");
  instance->shutdown();
}

int main(int argc, char ** argv)
{
  auto opts = rfaas::resource_manager::opts(argc, argv);
  if(opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%H:%M:%S:%f] [P %P] [T %t] [%l] %v ");
  spdlog::info("Executing rFaaS resource manager!");

  // Catch SIGINT
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = &signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);

  std::ifstream in_cfg{opts.json_config};
  rfaas::resource_manager::Settings settings = rfaas::resource_manager::Settings::deserialize(in_cfg);

  rfaas::resource_manager::Manager mgr(settings);
  instance = &mgr;

  if(opts.initial_database != "") {
    mgr.read_database(opts.initial_database);
  }
  if(opts.output_database != "") {
    mgr.set_database_path(opts.output_database);
  }

  mgr.start();

  spdlog::info("Resource manager is closing down");
  mgr.dump_database();

  return 0;
}
