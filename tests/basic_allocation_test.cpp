
#include "rfaas/devices.hpp"
#include <fstream>

#include <rfaas/rfaas.hpp>

#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>

struct Settings
{
  static constexpr char FLIB_PATH[] = "examples/libfunctions.so";
};

class BasicAllocationTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    {
      // Read connection details to the managers
      std::ifstream in_cfg("servers.json");
      rfaas::servers::deserialize(in_cfg);
    }

    {
      // Read device details to the managers
      std::ifstream in_cfg("devices.json");
      rfaas::devices::deserialize(in_cfg);
    }
  }

  void TearDown() override
  {

  }
};

// Test should execute without any exceptions.
TEST_F(BasicAllocationTest, BasicAllocation) {
  rfaas::devices & dev = rfaas::devices::instance();
  rfaas::executor executor(*dev.device("rocep7s0"));
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
  rfaas::executor executor(*dev.device("rocep7s0"));
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

