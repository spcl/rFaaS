
#ifndef __RDMALIB_BENCHMARKER_HPP__
#define __RDMALIB_BENCHMARKER_HPP__

#include <numeric>
#include <vector>
#include <string>
#include <tuple>
#include <chrono>
#include <fstream>

//#include <sys/time.h>

namespace rdmalib {

  template<int Cols>
  struct Benchmarker {
    std::vector<std::array<uint64_t, Cols>> _measurements;
    std::chrono::time_point<std::chrono::high_resolution_clock> _start, _end;

    Benchmarker(int measurements)
    {
      _measurements.reserve(measurements);
    }

    inline void start()
    {
      _start = std::chrono::high_resolution_clock::now();
    }

    inline uint64_t end(int col = 0)
    {
      _end = std::chrono::high_resolution_clock::now();
      uint64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(_end - _start).count();
      if(col == 0)
        _measurements.emplace_back();
      _measurements.back()[col] = duration;
      return duration;
    }

    void remove_last()
    {
      _measurements.pop_back();
    }

    std::tuple<double, double> summary(int idx = 0)
    {
      // FIXME: reenable
      long sum = std::accumulate(_measurements.begin(), _measurements.end(), 0L,
        [idx](long x, const std::array<uint64_t, Cols> & y) {
          return x + y[idx];
        }
      );
      double avg = static_cast<double>(sum) / _measurements.size();

      //// compute median
      //// let's just ignore the rule that for even size we should take an average of middle elements
      int middle = _measurements.size() / 2;
      std::nth_element(_measurements.begin(), _measurements.begin() + middle, _measurements.end(),
        [idx](const std::array<uint64_t, Cols> & x, const std::array<uint64_t, Cols> & y) {
          return x[idx] < y[idx];
        }
      );
      int median = _measurements[middle][idx];

      return std::make_tuple(static_cast<double>(median) / 1000, avg / 1000);
    }

    void export_csv(std::string fname, const std::array<std::string, Cols> & headers)
    {
      std::ofstream of(fname);
      of << "id";
      for(int j = 0; j < Cols; ++j)
        of << ',' << headers[j];
      of << '\n'; 

      for(size_t i = 0; i < _measurements.size(); ++i) {
        of << i;
        for(int j = 0; j < Cols; ++j)
          of <<  ',' << _measurements[i][j];
        of << '\n';
      }
    }

  };

}

#endif

