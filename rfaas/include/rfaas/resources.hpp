
#ifndef __RFAAS_RESOURCES_HPP__
#define __RFAAS_RESOURCES_HPP__

#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <cstring>

#include <cereal/types/vector.hpp> 
#include <cereal/types/string.hpp>

namespace rfaas {

  struct server_data
  {
    static constexpr int ADDRESS_LENGTH = 16;
    static constexpr int NODE_NAME_LENGTH = 32;

    int32_t port;
    int16_t cores;
    int32_t memory;
    std::string address;
    std::string node;

    server_data();
    server_data(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory);

    template <class Archive>
    void save(Archive & ar) const
    {
      std::string addr{address};
      ar(CEREAL_NVP(node), CEREAL_NVP(port), CEREAL_NVP(cores), CEREAL_NVP(memory), cereal::make_nvp("address", addr));
    }

    template <class Archive>
    void load(Archive & ar )
    {
      ar(CEREAL_NVP(node), CEREAL_NVP(port), CEREAL_NVP(cores), CEREAL_NVP(memory), CEREAL_NVP(address));
    }
  };

  struct servers
  {
    static std::unique_ptr<servers> _instance;
    std::vector<server_data> _data; 

    servers(int positions = 0);

    server_data & server(int idx);
    size_t size() const;

    template <class Archive>
    void save(Archive & ar) const
    {
      ar(CEREAL_NVP(_data));
    }

    template <class Archive>
    void load(Archive & ar )
    {
      ar(CEREAL_NVP(_data));
    }

    static servers & instance();
    static void deserialize(std::istream & in);
    void read(std::istream & in);
    void write(std::ostream & out);
  };

};

#endif

