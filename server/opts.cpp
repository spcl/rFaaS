
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
      ("polling-mgr", "Polling manager: server, thread, server-notify", cxxopts::value<std::string>()->default_value("server"))
      ("polling-type", "Polling type: wc (work completions), dram", cxxopts::value<std::string>()->default_value("wc"))
      ("warmup-iters", "Number of warm-up iterations", cxxopts::value<int>()->default_value("1"))
      ("pin-threads", "Pin worker threads to CPU cores", cxxopts::value<int>()->default_value("-1"))
      ("max-inline-data", "Maximum size of inlined message", cxxopts::value<int>()->default_value("0"))
      ("x,requests", "Size of recv buffer", cxxopts::value<int>()->default_value("32"))
      ("func-size", "Size of functions library", cxxopts::value<int>())
      ("timeout", "Timeout for switching hot to warm polling; -1 always hot, 0 always warm", cxxopts::value<int>())
      ("s,size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("r,repetitions", "Repetitions to execute", cxxopts::value<int>()->default_value("1"))
      ("f,file", "Output server status.", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("mgr-address", "Use selected address", cxxopts::value<std::string>())
      ("mgr-port", "Use selected port", cxxopts::value<int>())
      ("mgr-secret", "Use selected port", cxxopts::value<int>())
      ("mgr-buf-addr", "Use selected port", cxxopts::value<uint64_t>())
      ("mgr-buf-rkey", "Use selected port", cxxopts::value<uint32_t>())
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.address = parsed_options["address"].as<std::string>();
    result.port = parsed_options["port"].as<int>();
    result.cheap_executors = parsed_options["cheap"].as<int>();
    result.fast_executors = parsed_options["fast"].as<int>();
    result.recv_buffer_size = parsed_options["requests"].as<int>();
    result.msg_size = parsed_options["size"].as<int>();
    result.repetitions = parsed_options["repetitions"].as<int>();
    result.warmup_iters = parsed_options["warmup-iters"].as<int>();
    result.verbose = parsed_options["verbose"].as<bool>();
    result.pin_threads = parsed_options["pin-threads"].as<int>();
    result.max_inline_data = parsed_options["max-inline-data"].as<int>();
    result.func_size = parsed_options["func-size"].as<int>();
    result.timeout = parsed_options["timeout"].as<int>();

    result.mgr_address = parsed_options["mgr-address"].as<std::string>();
    result.mgr_port = parsed_options["mgr-port"].as<int>();
    result.mgr_secret = parsed_options["mgr-secret"].as<int>();
    result.accounting_buffer_addr = parsed_options["mgr-buf-addr"].as<uint64_t>();
    result.accounting_buffer_rkey = parsed_options["mgr-buf-rkey"].as<uint32_t>();

    std::string polling_mgr = parsed_options["polling-mgr"].as<std::string>();
    if(polling_mgr == "server") {
      result.polling_manager = Options::PollingMgr::SERVER;
    } else if(polling_mgr == "server-notify") {
      result.polling_manager = Options::PollingMgr::SERVER_NOTIFY;
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

