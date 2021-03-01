
#ifndef __CLIENT_MEASUREMENTS_HPP__
#define __CLIENT_MEASUREMENTS_HPP__

#include <numeric>
#include <vector>
#include <string>
#include <tuple>
#include <chrono>

#include <sys/time.h>

namespace client {

  struct Benchmarker {
    std::vector<uint64_t> _measurements;
    //timeval _start, _end;
    std::chrono::time_point<std::chrono::high_resolution_clock> _start, _end;

    Benchmarker(int measurements)
    {
      _measurements.reserve(measurements);
    }

    inline void start()
    {
      _start = std::chrono::high_resolution_clock::now();
      //gettimeofday(&_start, nullptr);
    }

    inline void end()
    {
      //gettimeofday(&_end, nullptr);
      //_measurements.emplace_back((_end.tv_sec - _start.tv_sec) * 1000000 + (_end.tv_usec - _start.tv_usec));
      _end = std::chrono::high_resolution_clock::now();
      _measurements.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(_end - _start).count());
    }

    std::tuple<double, double> summary()
    {
      long sum = std::accumulate(_measurements.begin(), _measurements.end(), 0L);
      double avg = static_cast<double>(sum) / _measurements.size();

      // compute median
      // let's just ignore the rule that for even size we should take an average of middle elements
      int middle = _measurements.size() / 2;
      std::nth_element(_measurements.begin(), _measurements.begin() + middle, _measurements.end());
      int median = _measurements[middle];

      return std::make_tuple(static_cast<double>(median) / 1000, avg / 1000);
    }

    void export_csv(std::string fname)
    {
      std::ofstream of(fname);
      of << "id,latency_ns" << '\n';
      for(size_t i = 0; i < _measurements.size(); ++i)
        of << i << ',' <<  _measurements[i] << '\n';
    }

  };

}

#endif

