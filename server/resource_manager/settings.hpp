

#ifndef __RFAAS_RESOURCE_MANAGER_SETTINGS_HPP__
#define __RFAAS_RESOURCE_MANAGER_SETTINGS_HPP__

#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::resource_manager {

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    rfaas::device_data* device;
    
    std::string http_network_address;
    uint16_t http_network_port;

    int rdma_threads;
    uint32_t rdma_secret;
    bool rdma_sleep;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        cereal::make_nvp("rdma-threads", rdma_threads),
        cereal::make_nvp("rdma-secret", rdma_secret),
        cereal::make_nvp("rdma-sleep", rdma_sleep),
        CEREAL_NVP(http_network_address), CEREAL_NVP(http_network_port)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif

