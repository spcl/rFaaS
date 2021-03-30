
#ifndef __TESTS__WARM_BENCHMARKER_HPP__
#define __TESTS__WARM_BENCHMARKER_HPP__

#include <string>

namespace warm_benchmarker {

  struct Options {

    enum class PollingMgr {
      SERVER_NOTIFY=0,
      THREAD
    };

    enum class PollingType {
      WC=0,
      DRAM
    };

    std::string address;
    int port;
    int repetitions;
    int warmup_iters;
    int numcores;
    std::string fname;
    std::string flib;
    int input_size;
    std::string out_file;
    bool pin_threads;
    int recv_buf_size;
    int max_inline_data;
    bool verbose;
  };

  Options options(int argc, char ** argv);

}

#endif
