
#include <fstream>

#include <rfaas/rfaas.hpp>
#include <rfaas/devices.hpp>

#include "config.h"

#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>


class BasicAllocationTest : public ::testing::Test {

public:
  static std::string _device_name;

protected:
  void SetUp() override
  {
    {
      // Read connection details to the managers
      std::ifstream in_cfg(Settings::EXEC_DB_PATH);
      rfaas::servers::deserialize(in_cfg);
    }

    _device_name = Settings::TEST_DEVICE;

    {
      // Read device details to the managers
      std::ifstream in_cfg(Settings::DEVICE_JSON_PATH);
      rfaas::devices::deserialize(in_cfg);
    }
  }

  void TearDown() override
  {

  }
};
std::string BasicAllocationTest::_device_name;

// Test should execute without any exceptions.
TEST_F(BasicAllocationTest, BasicAllocation) {
  rfaas::devices & dev = rfaas::devices::instance();
  rfaas::executor executor(*dev.device(_device_name));
  int numcores = 1;
  int input_size = 1;

  bool result = executor.allocate(
    std::string{Settings::FLIB_PATH},
    numcores,
    input_size,
    rfaas::polling_type::HOT_ALWAYS,
    false
  );
  EXPECT_TRUE(result);
  executor.deallocate();
}

// Test should still exit gracefully
TEST_F(BasicAllocationTest, UnfinishedAllocation) {
  rfaas::devices & dev = rfaas::devices::instance();
  rfaas::executor executor(*dev.device(_device_name));
  int numcores = 1;
  int input_size = 1;

  bool result = executor.allocate(
    std::string{Settings::FLIB_PATH},
    numcores,
    input_size,
    rfaas::polling_type::HOT_ALWAYS,
    false
  );
  EXPECT_TRUE(result);
}

// FIXME: test two cores
// FIXME: test multiple cores
// FIXME: test multiple clients
// FIXME: too many cores
// FIXME: algorithm of allocating multiple executors
// FIXME: timeout

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  std::string arg{argc == 1 ? "" : argv[1]};
  BasicAllocationTest::_device_name = arg;
  return RUN_ALL_TESTS();
}

