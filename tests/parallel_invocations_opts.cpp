
#include <cxxopts.hpp>

#include "parallel_invocations.hpp"

namespace parallel_invocations {

  // Compilation time of client.cpp decreased from 11 to 1.5 seconds!!!

  Options options(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-client", "Invoke functions");
    options.add_options()
      ("a,address", "Use selected address.", cxxopts::value<std::string>())
      ("p,port", "Use selected port.", cxxopts::value<int>()->default_value("0"))
      ("r,repetitions", "Repetitions", cxxopts::value<int>())
      ("x,recv-buf-size", "Size of recv buffer", cxxopts::value<int>()->default_value("1"))
      ("c,cores", "Number of cores", cxxopts::value<int>())
      ("name", "Function name", cxxopts::value<std::string>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("f,file", "Server status", cxxopts::value<std::string>())
      ("o,out-file", "Output with statistics", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("warmup-iters", "Number of warm-up iterations", cxxopts::value<int>()->default_value("1"))
      ("max-inline-data", "Maximum size of inlined message", cxxopts::value<int>()->default_value("0"))
      ("pin-threads", "Pin worker threads to CPU cores.", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);
    Options result;
    result.address = parsed_options["address"].as<std::string>();
    result.port = parsed_options["port"].as<int>();
    result.repetitions = parsed_options["repetitions"].as<int>();
    result.warmup_iters = parsed_options["warmup-iters"].as<int>();
    result.fname = parsed_options["name"].as<std::string>();
    result.input_size = parsed_options["size"].as<int>();
    result.out_file = parsed_options["out-file"].as<std::string>();
    result.pin_threads = parsed_options["pin-threads"].as<bool>();
    result.max_inline_data = parsed_options["max-inline-data"].as<int>();
    result.recv_buf_size = parsed_options["recv-buf-size"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();;
    result.numcores = parsed_options["cores"].as<int>();;

    return result;
  }

}
