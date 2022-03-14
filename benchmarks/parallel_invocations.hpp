
#ifndef __TESTS__WARM_BENCHMARKER_HPP__
#define __TESTS__WARM_BENCHMARKER_HPP__

#include <string>

namespace parallel_invocations {

  struct Options {

    std::string json_config;
    std::string device_database;
    std::string executors_database;
    std::string output_stats;
    bool verbose;
    std::string fname;
    std::string flib;
    int input_size;
    int numcores;

  };

  Options options(int argc, char ** argv);

}

#endif
