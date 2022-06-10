
#ifndef __TOOLS_SIMULATOR_SIMULATOR_HPP__
#define __TOOLS_SIMULATOR_SIMULATOR_HPP__

#include <random>

#include <mpi.h>

#include "mpi_log.hpp"

namespace simulator {

  struct Options {

    int clients;
    int executors;
    int experiments;
    int repetitions;
    int seed;
    int cores_executor;
    int cores_to_allocate;

  };

  Options opts(int argc, char ** argv);

  struct Client
  {
    int _initial_seed;
    int _cores_to_allocate;
    MPI_Comm _comm;
    std::mt19937 _prng;
    std::vector<int> _random_seeds;
    std::vector<int> _executors;

    log::Logger & _logger;

    Client(int seed, int cores_to_allocate, MPI_Comm comm, log::Logger & logger):
      _initial_seed(seed),
      _cores_to_allocate(cores_to_allocate),
      _comm(comm),
      _prng(_initial_seed),
      _logger(logger)
    {}

    void initialize_seeds(int iterations);
    void shuffle_executors(int low, int high, int iteration);
    void allocate();
  };

  struct Executor
  {
    int _cores;
    MPI_Comm _comm;

    log::Logger & _logger;

    Executor(int cores, MPI_Comm comm, log::Logger & logger):
      _cores(cores),
      _comm(comm),
      _logger(logger)
    {}

    void handle_requests();
  };

  struct RequestMessage
  {
    int _buffer;
    int _tag;
    MPI_Comm _comm;

    RequestMessage(MPI_Comm comm):
      _tag(0),
      _comm(comm)
    {}

    void set_allocation(int);
    int get_allocation();
    MPI_Datatype datatype();

    void send_allocation(int executor);
    void recv_allocation(MPI_Request*);
  };

  struct ReplyMessage
  {
    int _buffer;
    int _tag;
    MPI_Comm _comm;

    ReplyMessage(MPI_Comm comm):
      _tag(0),
      _comm(comm)
    {}

    void set_cores(int);
    void set_failure();
    bool succeeded();
    int get_cores();
    MPI_Datatype datatype();

    void send_reply(int client);
    void recv_reply(int executor);
  };

}

#endif

