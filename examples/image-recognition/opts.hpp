
#ifndef __EXAMPLES_THUMBNAILER_HPP__
#define __EXAMPLES_THUMBNAILER_HPP__

#include <string>


struct Options {

  std::string address;
  int port;
  int repetitions;
  int warmup_iters;
  int numcores;
  int hot_timeout;
  std::string fname;
  std::string flib;
  std::string image;
  std::string server_file;
  std::string out_file;
  bool pin_threads;
  int recv_buf_size;
  int max_inline_data;
  bool verbose;
};

Options options(int argc, char ** argv);

#endif
