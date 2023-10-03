
#include <algorithm>

#include <cereal/archives/json.hpp>

#include <rfaas/resources.hpp>

namespace rfaas {

  std::unique_ptr<servers> servers::_instance = nullptr;

  server_data::server_data():
    port(-1),
    cores(-1),
    memory(-1)
  {}

  server_data::server_data(const std::string & node, const std::string & ip, int32_t port, int16_t cores, int32_t memory):
    port(port),
    cores(cores),
    memory(memory),
    address(ip),
    node(node)
  {}

  servers::servers(int positions)
  {
    if(positions)
      _data.resize(positions);
  }

  server_data & servers::server(int idx)
  {
    return _data[idx];
  }

  size_t servers::size() const
  {
    return _data.size();
  }

  servers & servers::instance()
  {
    return *_instance.get();
  }

  void servers::deserialize(std::istream & in)
  {
    servers::_instance.reset(new servers{});
    servers::_instance.get()->read(in);
  }

  void servers::read(std::istream & in)
  {
    cereal::JSONInputArchive archive_in(in);
    archive_in(cereal::make_nvp("executors", this->_data));
  }

  void servers::write(std::ostream & out)
  {
    cereal::JSONOutputArchive archive_out(out);
    archive_out(cereal::make_nvp("executors", this->_data));
  }

}
