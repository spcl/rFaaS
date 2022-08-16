
#include <iostream>

#include <cxxopts.hpp>

#include "cold_benchmark.hpp"

namespace cold_benchmarker {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-cold-benchmarker", "Benchmark cold invocations");
    options.add_options()
      ("c,config", "JSON input config.",  cxxopts::value<std::string>())
      ("device-database", "JSON configuration of devices.", cxxopts::value<std::string>())
      ("executors-database", "JSON configuration of executor servers.", cxxopts::value<std::string>()->default_value(""))
      ("output-stats", "Output file for benchmarking statistics.", cxxopts::value<std::string>()->default_value(""))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("name", "Function name", cxxopts::value<std::string>())
      ("functions", "Functions library", cxxopts::value<std::string>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("h,help", "Print usage", cxxopts::value<bool>()->default_value("false"))
      ("cores", "Number of cores", cxxopts::value<int>()->default_value("1"))
      ("pause", "Pause between iterations [ms]", cxxopts::value<int>()->default_value("0"))
    ;
    auto parsed_options = options.parse(argc, argv);
    if(parsed_options.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    Options result;
    result.json_config = parsed_options["config"].as<std::string>();
    result.device_database = parsed_options["device-database"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();;
    result.fname = parsed_options["name"].as<std::string>();
    result.flib = parsed_options["functions"].as<std::string>();
    result.input_size = parsed_options["size"].as<int>();
    result.output_stats = parsed_options["output-stats"].as<std::string>();
    result.executors_database = parsed_options["executors-database"].as<std::string>();
    result.cores = parsed_options["cores"].as<int>();
    result.pause = parsed_options["pause"].as<int>();

    return result;
  }  

}
