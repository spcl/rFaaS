
#include <cxxopts.hpp>

#include "cold_benchmark.hpp"

namespace cold_benchmarker {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-cold-benchmarker", "Benchmark cold invocations");
    options.add_options()
      ("a,address", "Use selected address.", cxxopts::value<std::string>())
      ("p,port", "Use selected port.", cxxopts::value<int>()->default_value("0"))
      ("r,repetitions", "Repetitions", cxxopts::value<int>())
      ("warmup-iters", "Number of warm-up iterations", cxxopts::value<int>()->default_value("1"))
      ("fname", "Function name", cxxopts::value<std::string>())
      ("flib", "Functions library", cxxopts::value<std::string>())
      ("f,file", "Server status", cxxopts::value<std::string>())
      ("o,out-file", "Output with statistics", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("max-inline-data", "Maximum size of inlined message", cxxopts::value<int>()->default_value("0"))
      ("recv-buf-size", "Size of recv buffer", cxxopts::value<int>()->default_value("1"))
      ("c,cores", "Number of cores", cxxopts::value<int>())
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.address = parsed_options["address"].as<std::string>();
    result.port = parsed_options["port"].as<int>();
    result.repetitions = parsed_options["repetitions"].as<int>();
    result.warmup_iters = parsed_options["warmup-iters"].as<int>();
    result.cores = parsed_options["cores"].as<int>();
    result.fname = parsed_options["fname"].as<std::string>();
    result.flib = parsed_options["flib"].as<std::string>();
    result.input_size = parsed_options["size"].as<int>();
    result.server_file = parsed_options["file"].as<std::string>();
    result.out_file = parsed_options["out-file"].as<std::string>();
    result.max_inline_data = parsed_options["max-inline-data"].as<int>();
    result.recv_buf_size = parsed_options["recv-buf-size"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();;

    return result;
  }  

}
