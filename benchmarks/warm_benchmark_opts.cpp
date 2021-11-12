
#include <cxxopts.hpp>

#include "warm_benchmark.hpp"

namespace warm_benchmarker {

  // Compilation time of client.cpp decreased from 11 to 1.5 seconds!!!

  Options options(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-client", "Invoke functions");
    options.add_options()
      ("c,config", "JSON input config.",  cxxopts::value<std::string>())
      ("device-database", "JSON configuration of devices.", cxxopts::value<std::string>())
      ("executors-database", "JSON configuration of executor servers.", cxxopts::value<std::string>()->default_value(""))
      ("output-stats", "Output file for benchmarking statistics.", cxxopts::value<std::string>()->default_value(""))
      ("skip-resource-manager", "Ignore resource manager and don't connect to it.", cxxopts::value<bool>()->default_value("false"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("name", "Function name", cxxopts::value<std::string>())
      ("functions", "Functions library", cxxopts::value<std::string>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
    ;
    auto parsed_options = options.parse(argc, argv);
    Options result;
    result.json_config = parsed_options["config"].as<std::string>();
    result.device_database = parsed_options["device-database"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();;
    result.fname = parsed_options["name"].as<std::string>();
    result.flib = parsed_options["functions"].as<std::string>();
    result.input_size = parsed_options["size"].as<int>();
    result.output_stats = parsed_options["output-stats"].as<std::string>();
    result.executors_database = parsed_options["executors-database"].as<std::string>();

    return result;
  }

}
