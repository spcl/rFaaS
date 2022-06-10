
#ifndef __TOOLS_SIMULATOR_EXECUTORS_HPP__
#define __TOOLS_SIMULATOR_EXECUTORS_HPP__


#include <random>
#include <vector>

namespace simulator {

  struct Executors
  {
    typedef std::vector<int> value_t;

    int _initial_seed;
    std::mt19937 _prng;
    std::vector<int> _random_seeds;
    value_t _executors;

    Executors(int seed):
      _initial_seed(seed),
      _prng(_initial_seed)
    {}

    const value_t & executors() const
    {
      return _executors;
    }

    void initialize_seeds(int iterations);
    void shuffle_executors(int low, int high, int iteration);
  };

}

#endif

