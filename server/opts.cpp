
#include <cxxopts.hpp>

#include "server.hpp"

namespace server {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-server", "Handle functions invocations.");
    options.add_options()
      ("a,address", "Use selected address", cxxopts::value<std::string>())
      ("p,port", "Use selected port", cxxopts::value<int>()->default_value("0"))
      ("cheap", "Number of cheap executors", cxxopts::value<int>()->default_value("0"))
      ("fast", "Number of fast executors", cxxopts::value<int>()->default_value("1"))
      ("polling-mgr", "Polling manager: server, thread", cxxopts::value<std::string>()->default_value("server"))
      ("polling-type", "Polling type: wc (work completions), dram", cxxopts::value<std::string>()->default_value("wc"))
      ("warmup-iters", "Number of warm-up iterations", cxxopts::value<int>()->default_value("1"))
      ("pin-threads", "Pin worker threads to CPU cores", cxxopts::value<bool>()->default_value("false"))
      ("x,requests", "Size of recv buffer", cxxopts::value<int>()->default_value("32"))
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("r,repetitions", "Repetitions to execute", cxxopts::value<int>()->default_value("1"))
      ("f,file", "Output server status.", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.address = parsed_options["address"].as<std::string>();
    result.port = parsed_options["port"].as<int>();
    result.cheap_executors = parsed_options["cheap"].as<int>();
    result.fast_executors = parsed_options["fast"].as<int>();
    result.recv_buffer_size = parsed_options["requests"].as<int>();
    result.msg_size = parsed_options["size"].as<int>();
    result.server_file = parsed_options["file"].as<std::string>();
    result.repetitions = parsed_options["repetitions"].as<int>();
    result.warmup_iters = parsed_options["warmup-iters"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();
    result.pin_threads = parsed_options["pin-threads"].as<bool>();

    std::string polling_mgr = parsed_options["polling-mgr"].as<std::string>();
    if(polling_mgr == "server") {
      result.polling_manager = Options::PollingMgr::SERVER;
    } else if(polling_mgr == "thread") {
      result.polling_manager = Options::PollingMgr::THREAD;
    } else {
      throw std::runtime_error("Unrecognized choice for polling-mgr option: " + polling_mgr);
    }

    std::string polling_type = parsed_options["polling-type"].as<std::string>();
    if(polling_type == "wc") {
      result.polling_type = Options::PollingType::WC;
    } else if(polling_type == "dram") {
      result.polling_type = Options::PollingType::DRAM;
    } else {
      throw std::runtime_error("Unrecognized choice for polling-type option: " + polling_type);
    }

    return result;
  }
}

