
#ifndef __TESTS__CPP_INTERFACE_HPP__
#define __TESTS__CPP_INTERFACE_HPP__

#include <string>

namespace cpp_interface {

  struct Options {

    std::string json_config;
    std::string device_database;
    std::string executors_database;
    std::string output_stats;
    bool verbose;
    std::string fname;
    std::string flib;
    int input_size;

  };

  Options options(int argc, char ** argv);

}

#endif
