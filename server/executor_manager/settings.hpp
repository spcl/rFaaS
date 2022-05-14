

#ifndef __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__
#define __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__

#include <map>
#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::executor_manager {

  enum class SandboxType {
    PROCESS = 0,
    DOCKER = 1,
    SARUS = 2
  };

  SandboxType sandbox_deserialize(std::string type);

  std::string sandbox_serialize(SandboxType type);

  struct ExecutorSettings
  {
    SandboxType sandbox_type;
    int repetitions;
    int warmup_iters;
    int recv_buffer_size;
    int max_inline_data;
    bool pin_threads;

    template <class Archive>
    void load(Archive & ar )
    {
      std::string sandbox_type;
      ar(
        CEREAL_NVP(sandbox_type), CEREAL_NVP(repetitions),
        CEREAL_NVP(warmup_iters), CEREAL_NVP(pin_threads)
      );
      this->sandbox_type = sandbox_deserialize(sandbox_type);
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
    int resource_manager_secret;

    // Passed to the scheduled executor
    ExecutorSettings exec;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(resource_manager_address), CEREAL_NVP(resource_manager_port),
        CEREAL_NVP(resource_manager_secret)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif
