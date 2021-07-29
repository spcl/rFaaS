
#include "rfaas/devices.hpp"
#include <fstream>

#include <rfaas/rfaas.hpp>

#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>

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


// Demonstrate some basic assertions.
TEST_F(BasicAllocationTest, BasicAllocation) {
  rfaas::devices & dev = rfaas::devices::instance();
  rfaas::executor executor(*dev.device("rocep7s0"));
  //executor.allocate(opts.flib, opts.numcores, opts.input_size, opts.hot_timeout, false);
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}

