
#ifndef __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__
#define __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__

#include <map>
#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>

namespace rfaas::executor_manager {

  enum class SandboxType {
    PROCESS = 0,
    DOCKER = 1,
    SARUS = 2,
    SINGULARITY = 3,
  };

  SandboxType sandbox_deserialize(std::string type);

  std::string sandbox_serialize(SandboxType type);

}

namespace rfaas::executor_manager {

  struct DockerConfiguration {
    std::string image;
    std::string network;
    std::string ip;
    std::string volume;
    std::string registry_ip;
    int registry_port;

    template <class Archive>
    void load(Archive & ar)
    {
      ar(
        CEREAL_NVP(image), CEREAL_NVP(network),
        CEREAL_NVP(ip), CEREAL_NVP(volume),
        CEREAL_NVP(registry_ip), CEREAL_NVP(registry_port)
      );
    }

    void generate_args(std::vector<std::string> & args) const;
  };

  struct SarusConfiguration {
    std::string user;
    std::string name;
    std::vector<std::string> devices;
    std::vector<std::string> mounts;
    std::vector<std::string> mount_filesystem;
    std::map<std::string, std::string> env;

    template <class Archive>
    void load(Archive & ar)
    {
      ar(
        CEREAL_NVP(user), CEREAL_NVP(name),
        CEREAL_NVP(devices), CEREAL_NVP(mounts),
        CEREAL_NVP(mount_filesystem), CEREAL_NVP(env)
      );
    }

    void generate_args(std::vector<std::string> & args, const std::string & user) const;

    /**
     * In the Sarus container, we cannot build the executor since we need to
     * compile with Cray headers.
     * Thus, we have to mount the rFaaS build directory and access executor
     * this way.
     **/
    std::string get_executor_path() const;
  
  };

  struct SingularityConfiguration {
    // TODO: Add other singularity options here
    std::string container;

    template <class Archive>
    void load(Archive & ar)
    {
      ar(
        CEREAL_NVP(container)
      );
    }
  };

  using SandboxConfiguration = std::variant<DockerConfiguration, SarusConfiguration,
        SingularityConfiguration>;

  struct ExecutorSettings
  {
    SandboxType sandbox_type;
    SandboxConfiguration* sandbox_config;
    std::string sandbox_user;
    std::string sandbox_name;

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

    std::map<SandboxType, SandboxConfiguration> sandboxes;

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

namespace cereal
{

  template <class Archive> inline
  std::string save_minimal(Archive const &, rfaas::executor_manager::SandboxType const & t)
  {
    return rfaas::executor_manager::sandbox_serialize(t);
  }

  template <class Archive> inline
  void load_minimal( Archive const &, rfaas::executor_manager::SandboxType & t, std::string const & value)
  {
    t = rfaas::executor_manager::sandbox_deserialize(value);
  }

}

#endif

