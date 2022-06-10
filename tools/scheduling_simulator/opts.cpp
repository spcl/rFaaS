
#include <cxxopts.hpp>

#include "simulator.hpp"

namespace simulator {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("scheduling-simulator", "Benchmark cold invocations");
    options.add_options()
      ("clients", "Number of clients", cxxopts::value<int>())
      ("executors", "Number of executors", cxxopts::value<int>())
      ("experiments", "Number of experiments to run", cxxopts::value<int>())
      ("repetitions", "Number of repetitions for each experiment", cxxopts::value<int>())
      ("seed", "Starting random seed.", cxxopts::value<int>())
      ("cores-executor", "Number of cores on each executor", cxxopts::value<int>())
      ("cores-to-allocate", "Number of cores to allocate on each client", cxxopts::value<int>())
      ("output", "Output directory.", cxxopts::value<std::string>())
    ;
    auto parsed_options = options.parse(argc, argv);
    if(parsed_options.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    Options result;
    result.clients = parsed_options["clients"].as<int>();
    result.executors = parsed_options["executors"].as<int>();
    result.experiments = parsed_options["experiments"].as<int>();
    result.repetitions = parsed_options["repetitions"].as<int>();
    result.seed = parsed_options["seed"].as<int>();
    result.cores_executor = parsed_options["cores-executor"].as<int>();
    result.cores_to_allocate = parsed_options["cores-to-allocate"].as<int>();
    result.output = parsed_options["output"].as<std::string>();

    return result;
  }  

}
