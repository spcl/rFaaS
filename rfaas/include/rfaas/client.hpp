
#ifndef __RFAAS_RFAAS_HPP__
#define __RFAAS_RFAAS_HPP__

#include "rdmalib/connection.hpp"
#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>
#include <rfaas/devices.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>
#include <spdlog/spdlog.h>

namespace rfaas {

  struct client {

    client(std::string address, int port, device_data & dev):
      _resource_mgr(address, port, dev.default_receive_buffer_size, dev.max_inline_data),
      _device(dev)
    {}

    bool connect()
    {
      return _resource_mgr.connect();
    }

    std::optional<rfaas::executor> lease(int16_t cores, int32_t memory)
    { 
      if(!_resource_mgr.connected()) {
        return std::nullopt;
      }

      _resource_mgr.request() = (rfaas::LeaseRequest) {
        cores,
        memory
      };
      _resource_mgr.submit();

      auto [responses, response_count] = _resource_mgr.connection().poll_wc(rdmalib::QueueType::RECV, true);

      if(response_count > 1) {
        spdlog::warn("Received unexpected responses from resource manager, ignoring {} responses", response_count - 1);
      }
    
      std::cerr << responses[0].wr_id << " " << responses[0].imm_data << " " << ntohl(responses[0].imm_data) << std::endl;
      int executors = ntohl(responses[0].imm_data);
      std::cerr << "executors: " << executors << std::endl;
      std::cerr << _resource_mgr.response(0).nodes[0].address << std::endl;
      std::cerr << _resource_mgr.response(0).nodes[0].cores << std::endl;
      std::cerr << _resource_mgr.response(0).nodes[0].port << std::endl;

      //rfaas::executor executor(settings.device->ip_address,
      //                        settings.rdma_device_port,
      //                        settings.device->default_receive_buffer_size,
      //                        settings.device->max_inline_data);

      return std::nullopt;
    }

    std::optional<rfaas::executor> lease(servers & nodes_data, int16_t cores, int32_t memory)
    {
      if(!nodes_data.size()) {
        return std::nullopt;
      }

      server_data instance = nodes_data.server(0);

      return std::make_optional<rfaas::executor>(instance.address, instance.port, cores, memory, _device);
    }

  private:
    resource_mgr_connection _resource_mgr;

    device_data _device;
  };

}

#endif
