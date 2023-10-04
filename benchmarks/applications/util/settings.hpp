
#ifndef __BENCHMARK_SETTINGS_HPP__
#define __BENCHMARK_SETTINGS_HPP__

#include <string>
#include <vector>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>
#include <spdlog/spdlog.h>

namespace rfaas::application {

  struct BenchmarkSettings
  {
    int repetitions;
    int size;
    std::string library;
    std::string outfile;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(repetitions), CEREAL_NVP(size),
        CEREAL_NVP(library), CEREAL_NVP(outfile)
      );
    }
  };

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    rfaas::device_data* device;

    // resource manager connection
    std::string resource_manager_address;
    int resource_manager_port;

    // used if resource manager cannot be deployed for any reason
    std::vector<std::string> executor_databases;
    std::string device_databases;

    // Passed to the scheduled executor
    BenchmarkSettings benchmark;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(resource_manager_address), CEREAL_NVP(resource_manager_port),
        CEREAL_NVP(executor_databases), CEREAL_NVP(device_databases)
      );
    }

    void initialize_device()
    {
      // read RDMA device details
      rfaas::device_data * dev = rfaas::devices::instance().device(this->rdma_device);
      if(!dev) {
        spdlog::error("Data for device {} not found!", this->rdma_device);
        throw std::runtime_error{"Unknown device!"};
      }
      this->device = dev;
    }

    static Settings deserialize(std::istream & in)
    {

      Settings settings{};
      cereal::JSONInputArchive archive_in(in);
      archive_in(cereal::make_nvp("config", settings));
      archive_in(cereal::make_nvp("benchmark", settings.benchmark));

      return settings;

    }
  };

}

#endif
