
#include <cereal/archives/json.hpp>

#include <rfaas/devices.hpp>
#include <algorithm>

namespace rfaas {

std::unique_ptr<devices> devices::_instance = nullptr;

device_data *devices::device(std::string name) noexcept {
  auto it = std::find_if(_data.begin(), _data.end(), [name](device_data &data) {
    return data.name == name;
  });
  return it != _data.end() ? &*it : nullptr;
}

device_data *devices::front() noexcept {
  return !_data.empty() ? &_data.front() : nullptr;
}

devices &devices::instance() { return *_instance.get(); }

void devices::deserialize(std::istream &in) {
  devices::_instance.reset(new devices{});
  cereal::JSONInputArchive archive_in(in);
  // archive_in(cereal::make_nvp("devices", *devices::_instance.get()));
  archive_in(cereal::make_nvp("devices", devices::_instance.get()->_data));
}
//    void epilogue(cereal::JSONInputArchive& ar, const device_data&) {
//    std::cout << "test " << ar.getNodeName() << std::endl;
//  }
//
//    void prologue(cereal::JSONInputArchive& ar, const device_data&) {
//    std::cout << "test " << ar.getNodeName() << std::endl; } void
//    epilogue(cereal::JSONInputArchive& ar, const devices&) { std::cout <<
//    "test2 " << ar.getNodeName() << std::endl;
//  }
//
//    void prologue(cereal::JSONInputArchive& ar, const devices&) { std::cout <<
//    "test " << ar.getNodeName() << std::endl; }
//
} // namespace rfaas

// void epilogue(cereal::JSONInputArchive& ar, const rfaas::device_data&) {
//   std::cout << ar.getNodeName() << std::endl;
// }
// void prologue(cereal::JSONInputArchive& ar, const rfaas::device_data&) {
//   std::cout << ar.getNodeName() << std::endl;
// }

namespace cereal {

// void epilogue(cereal::JSONInputArchive&, const rfaas::device_data&) {}
// void prologue(cereal::JSONInputArchive& ar, const rfaas::device_data&) {
// std::cout << ar.getNodeName() << std::endl; }

}
