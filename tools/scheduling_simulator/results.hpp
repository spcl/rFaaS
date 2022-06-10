
#ifndef __TOOLS_SIMULATOR_RESULTS_HPP__
#define __TOOLS_SIMULATOR_RESULTS_HPP__

#include <vector>
#include <chrono>

#include <cereal/types/vector.hpp> 

#include "executors.hpp"

namespace simulator {

  struct ClientResults
  {
    typedef typename Executors::value_t executors_t;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> timepoint_t;

    struct Iteration
    {
      double time_us;
      int failed_allocations;
      int succesful_allocations;
      bool completed;

      template <class Archive>
      void save(Archive & ar) const
      {
        ar(
          CEREAL_NVP(time_us), CEREAL_NVP(failed_allocations),
          CEREAL_NVP(succesful_allocations), CEREAL_NVP(completed)
        );
      }
    };

    struct Result {
      executors_t executors;
      std::vector<Iteration> iterations;

      Result(const executors_t & executors_data):
        executors(executors_data)
      {}

      template <class Archive>
      void save(Archive & ar) const
      {
        ar(CEREAL_NVP(executors), CEREAL_NVP(iterations));
      }
    };

    std::vector<Result> experiments;
    Iteration _iter;
    timepoint_t _iter_begin;

    void begin_experiment(const Executors & executors)
    {
      this->experiments.emplace_back(executors.executors());
    }

    void start_iteration()
    {
      _iter = Iteration{0.0, 0, 0, 0};
      _iter_begin = std::chrono::high_resolution_clock::now();
    }

    void failed_allocation()
    {
      _iter.failed_allocations += 1;
    }

    void partial_allocation()
    {
      _iter.succesful_allocations += 1;
    }

    void complete_allocation(bool success)
    {
      timepoint_t end = std::chrono::high_resolution_clock::now();

      _iter.completed = success;
      _iter.time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - _iter_begin).count();

      this->experiments.back().iterations.push_back(std::move(_iter));
    }

    template <class Archive>
    void save(Archive & ar) const
    {
      ar(CEREAL_NVP(experiments));
    }

  };

  struct ExecutorResults
  {
    typedef typename Executors::value_t executors_t;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> timepoint_t;

    struct Request
    {
      double timestamp;
      int cores;
      bool accepted;

      Request(double timestamp, int cores, bool accepted):
        timestamp(timestamp),
        cores(cores),
        accepted(accepted)
      {}

      template <class Archive>
      void save(Archive & ar) const
      {
        ar(
          CEREAL_NVP(timestamp), CEREAL_NVP(cores), CEREAL_NVP(accepted)
        );
      }
    };

    struct Iteration
    {
      std::vector<Request> request_timestamps;

      template <class Archive>
      void save(Archive & ar) const
      {
        ar(CEREAL_NVP(request_timestamps));
      }
    };

    struct Result {
      std::vector<Iteration> iterations;

      template <class Archive>
      void save(Archive & ar) const
      {
        ar(CEREAL_NVP(iterations));
      }
    };

    std::vector<Result> experiments;
    Iteration _iter;
    timepoint_t _iter_begin;

    void begin_experiment()
    {
      this->experiments.emplace_back();
    }

    void start_iteration()
    {
      _iter = Iteration{};
      _iter_begin = std::chrono::high_resolution_clock::now();
    }

    void end_iteration()
    {
      this->experiments.back().iterations.push_back(std::move(_iter));
    }

    void register_request(int cores, bool accepted)
    {
      timepoint_t end = std::chrono::high_resolution_clock::now();
      double time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - _iter_begin).count();
      _iter.request_timestamps.emplace_back(time_us, cores, accepted);
    }

    template <class Archive>
    void save(Archive & ar) const
    {
      ar(CEREAL_NVP(experiments));
    }

  };

}

#endif
