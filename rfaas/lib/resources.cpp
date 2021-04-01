
#include <algorithm>
#include <cereal/archives/json.hpp>

#include <rfaas/resources.hpp>

namespace rfaas {

  std::unique_ptr<servers> servers::_instance = nullptr;

  servers::servers(int positions)
  {
    if(positions)
      _data.resize(positions);
  }

  server_data & servers::server(int idx)
  {
    return _data[idx];
  }

  std::vector<int> servers::select(int cores)
  {
    // FIXME: random walk
    // FIXME: take size of server in account
    return {0};
  }

  servers & servers::instance()
  {
    return *_instance.get();
  }

  void servers::deserialize(std::istream & in)
  {
    servers::_instance.reset(new servers{});
    cereal::JSONInputArchive archive_in(in);
    archive_in(*servers::_instance.get());
  }

}
