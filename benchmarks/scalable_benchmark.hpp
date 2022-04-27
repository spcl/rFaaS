
#ifndef __TESTS__COLD_BENCHMARKER_HPP__
#define __TESTS__COLD_BENCHMARKER_HPP__

#include <string>

namespace scalable_benchmarker {

  struct Options {

    std::string json_config;
    std::string device_database;
    std::string executors_database;
    std::string output_stats;
    bool verbose;
    std::string fname;
    std::string flib;
    int input_size;
    int cores;
    int fail;

  };

  Options opts(int argc, char ** argv);

}

#endif

