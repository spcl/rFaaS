
#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include "settings.hpp"

namespace rfaas::benchmark {

  Settings Settings::deserialize(std::istream & in)
  {
    Settings settings{};
    cereal::JSONInputArchive archive_in(in);
    archive_in(cereal::make_nvp("config", settings));
    archive_in(cereal::make_nvp("benchmark", settings.benchmark));

    // read RDMA device details
    rfaas::device_data * dev = rfaas::devices::instance().device(settings.rdma_device);
    if(!dev) {
      spdlog::error("Data for device {} not found!", settings.rdma_device);
      throw std::runtime_error{"Unknown device!"};
    }
    settings.device = dev;

    return settings;
  }

  Options options(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-client", "Invoke functions");
    options.add_options()
      ("h,help", "Print usage", cxxopts::value<bool>()->default_value("false"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("c,config", "JSON input config",  cxxopts::value<std::string>())
      ("device-database", "JSON configuration of devices", cxxopts::value<std::string>())
      ("executors-database", "JSON configuration of executor servers", cxxopts::value<std::string>()->default_value(""))
      ("output-stats", "Output file for benchmark statistics", cxxopts::value<std::string>()->default_value(""))
      ("name", "Function name", cxxopts::value<std::string>())
      ("functions", "Functions library", cxxopts::value<std::string>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("pause", "Pause between iterations [ms]", cxxopts::value<int>()->default_value("50"))
      ("rma_memory", "RMA function: Size of remote memory in [bytes]", cxxopts::value<int>()->default_value("1024"))
      ("rma_mode", "RMA function: For reading (0) or writing (1) the payload", cxxopts::value<int>()->default_value("0"))
      ("cores", "Number of cores", cxxopts::value<int>()->default_value("1"))
    ;
    auto parsed_options = options.parse(argc, argv);
    if(parsed_options.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    Options result;
    result.verbose = parsed_options["verbose"].as<bool>();
    result.json_config = parsed_options["config"].as<std::string>();
    result.device_database = parsed_options["device-database"].as<std::string>();
    result.executors_database = parsed_options["executors-database"].as<std::string>();
    result.output_stats = parsed_options["output-stats"].as<std::string>();
    result.fname = parsed_options["name"].as<std::string>();
    result.flib = parsed_options["functions"].as<std::string>();
    result.input_size = parsed_options["size"].as<int>();
    result.pause = parsed_options["pause"].as<int>();
    result.rma_memory = parsed_options["rma_memory"].as<int>();
    result.rma_mode = parsed_options["rma_mode"].as<int>();
    result.cores = parsed_options["cores"].as<int>();

    return result;
  }
}

