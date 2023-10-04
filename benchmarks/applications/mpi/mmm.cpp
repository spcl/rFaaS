
#include <chrono>
#include <fstream>
#include <vector>
#include <sys/time.h>
#include <unistd.h>

#include <mpi.h>

#include <rdmalib/functions.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

#include "mmm.h"
#include "settings.hpp"

int main(int argc, char ** argv)
{
  MPI_Init(&argc, &argv);

  spdlog::set_level(spdlog::level::debug);
  int rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  struct timeval tv1, tv2;
  struct timezone tz;
  std::string config{argv[1]};

  // Read benchmark settings
  std::ifstream benchmark_cfg{config};
  rfaas::application::Settings settings = rfaas::application::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  char hostname[32];
  gethostname(hostname, 32);

  // Read device details
  std::string dev_file = settings.device_databases + "/" + hostname + ".json";
  std::ifstream in_dev{dev_file};
  if(in_dev.fail()) {
    spdlog::error("Could not open {}", dev_file);
    abort();
  }
  rfaas::devices::deserialize(in_dev);
  in_dev.close();

  rdmalib::Configuration::get_instance().configure_cookie(
    rfaas::devices::instance()._configuration.authentication_credential
  );

  settings.initialize_device();

  int executors = settings.executor_databases.size();
  int ranks_per_executors = world_size / executors;
  int my_executor = rank / ranks_per_executors;
  std::string executor_database = settings.executor_databases[my_executor];
  std::string flib = settings.benchmark.library;

  std::cout << ("Rank " + std::to_string(rank) + " executing on " + std::string{hostname}
    + ", device " + settings.device->ip_address
     +  ", port " + std::to_string(settings.rdma_device_port + rank)
     + ", using servers db: " + executor_database + ", flib: " + flib) << '\n';

  std::ifstream in_servers(executor_database);
  if(in_servers.fail()) {
    spdlog::error("Could not open {}", executor_database);
    abort();
  }
  rfaas::servers::deserialize(in_servers);
  in_servers.close();

  rfaas::executor executor(
    settings.device->ip_address,
    settings.rdma_device_port + rank,
    settings.device->default_receive_buffer_size,
    settings.device->max_inline_data
  );
  int size = settings.benchmark.size;
  int reps = settings.benchmark.repetitions;
  //executor.allocate(env_flib, 1, 2*size*size*sizeof(double), -1, false);
  executor.allocate(flib, 1, 2*size*size*sizeof(double), 0, false);
  rdmalib::Buffer<double> input{size*size*2, rdmalib::functions::Submission::DATA_HEADER_SIZE};
  rdmalib::Buffer<double> output{size*size};
  #ifdef USE_LIBFABRIC
  input.register_memory(executor._state.pd(), FI_WRITE);
  output.register_memory(executor._state.pd(), FI_WRITE | FI_REMOTE_WRITE);
  #else
  input.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  output.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE| IBV_ACCESS_REMOTE_WRITE);
  #endif

  double* A = input.data();
  double* B = input.data() + size*size;
  double* C = output.data();
  double* C_local = new double[size*size]();
  for(int i = 0; i < size; i++)
  {
    for(int j = 0; j < size; j++)
    {
        A[i*size + j] = 1 + i;
        B[i*size + j] = 2 + j;
    }
  }

  std::vector<int> times;
  times.reserve(reps);
  printf("Multiply matrices of size %d and %d times...\n", size, reps);
  gettimeofday(&tv1, &tz);
  for (int i = 0; i < reps; i++)
  {
    memset(C_local, 0, sizeof(double) * size * size);
    auto b = std::chrono::high_resolution_clock::now();
    //std::cerr << "Submit" << std::endl;
    auto f = executor.async("mmm3", input, output);
    mmm2(size, A, B, C_local);
    //std::cerr << "wait" << std::endl;
    f.get();
    //executor.block();
    //std::cerr << "done" << std::endl;
    for(int j = 0; j < size; ++j)
      for(int k = 0; k < size; ++k)
        C[j*size + k] += C_local[j*size + k];
    auto e = std::chrono::high_resolution_clock::now();
    times.emplace_back( std::chrono::duration_cast<std::chrono::nanoseconds>(e - b).count() );
  }
  gettimeofday(&tv2, &tz);
  double elapsed = (double) (tv2.tv_sec-tv1.tv_sec) + (double) (tv2.tv_usec-tv1.tv_usec) * 1.e-6;
  printf("Time = %lf [ms] \n", elapsed/reps*1000);


  //for(int i = 0; i < size; i++)
  //{
  //  for(int j = 0; j < size; j++)
  //  {
  //    std::cout << C[i*size + j] << ' ';
  //  }
  //  std::cout << '\n';
  //}

  std::ofstream out(settings.benchmark.outfile + "_" + std::to_string(rank));
  out << "repetition\n";
  for(int t : times)
    out << t << '\n';
  out.close();

  executor.deallocate();

  MPI_Finalize();
}
