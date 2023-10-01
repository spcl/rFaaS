

#ifndef __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__
#define __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__

#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::executor_manager {

  struct ExecutorSettings
  {
    bool use_docker;
    int repetitions;
    int warmup_iters;
    int recv_buffer_size;
    int max_inline_data;
    bool pin_threads;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(use_docker), CEREAL_NVP(repetitions),
        CEREAL_NVP(warmup_iters), CEREAL_NVP(pin_threads)
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
    std::string node_name;
    bool rdma_sleep;

    // resource manager connection
    std::string resource_manager_address;
    int resource_manager_port;
    int resource_manager_secret;

    // Passed to the scheduled executor
    ExecutorSettings exec;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(node_name), cereal::make_nvp("rdma-sleep", rdma_sleep),
        CEREAL_NVP(resource_manager_address), CEREAL_NVP(resource_manager_port),
        CEREAL_NVP(resource_manager_secret)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif
