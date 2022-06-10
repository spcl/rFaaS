
#include <algorithm>

#include "executors.hpp"

namespace simulator {

  void Executors::initialize_seeds(int iterations)
  {
    _random_seeds.resize(iterations);
    std::generate_n(_random_seeds.begin(), iterations, _prng);
  }

  void Executors::shuffle_executors(int low, int high, int iteration)
  {
    _executors.resize(high - low);
    std::iota(_executors.begin(), _executors.end(), low);

    // shuffle
    _prng.seed(_random_seeds[iteration]);
    std::shuffle(_executors.begin(), _executors.end(), _prng);
  }

}

