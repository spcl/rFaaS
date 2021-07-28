
#ifndef __TESTS__COLD_BENCHMARKER_HPP__
#define __TESTS__COLD_BENCHMARKER_HPP__

#include <string>

namespace cold_benchmarker {

  struct Options {

    enum class PollingMgr {
      SERVER=0,
      SERVER_NOTIFY,
      THREAD
    };

    enum class PollingType {
      WC=0,
      DRAM
    };

    std::string address;
    int port;
    std::string server_file;
    int repetitions;
    int warmup_iters;
    int cores;
    int hot_timeout;
    std::string fname;
    std::string flib;
    int input_size;
    std::string out_file;
    int pause;
    bool pin_threads;
    int recv_buf_size;
    int max_inline_data;
    bool verbose;
  };

  Options opts(int argc, char ** argv);

}

#endif

