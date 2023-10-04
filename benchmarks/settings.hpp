

#ifndef __RFAAS_BENCHMARK_SETTINGS_HPP__
#define __RFAAS_BENCHMARK_SETTINGS_HPP__

#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::benchmark {

  struct BenchmarkSettings
  {
    int repetitions;
    int warmup_repetitions;
    bool pin_threads;
    int hot_timeout;
    int numcores;
    int memory;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(repetitions), CEREAL_NVP(warmup_repetitions),
        CEREAL_NVP(pin_threads), CEREAL_NVP(hot_timeout),
        CEREAL_NVP(numcores), CEREAL_NVP(memory)
      );
    }
  };

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    device_data* device;

    // resource manager connection
    std::string resource_manager_address;
    int resource_manager_port;

    // Passed to the scheduled executor
    BenchmarkSettings benchmark;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(resource_manager_address), CEREAL_NVP(resource_manager_port)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif
