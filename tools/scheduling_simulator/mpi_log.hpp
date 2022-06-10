
#ifndef __TOOLS_SIMULATOR_MPI_LOG_HPP__
#define __TOOLS_SIMULATOR_MPI_LOG_HPP__

#include <string>

#include <spdlog/spdlog.h>

namespace simulator { namespace log {

  struct Logger
  {
    int _rank;

    Logger(int rank):
      _rank(rank)
    {}

    template<typename... Args>
    void info(const std::string& msg, Args &&... args)
    {
      spdlog::info("[Rank {}] " + msg, _rank, args...);
    }

    template<typename... Args>
    void debug(const std::string& msg, Args &&... args)
    {
      SPDLOG_DEBUG("[Rank {}] " + msg, _rank, args...);
    }
  };

}}

#endif

