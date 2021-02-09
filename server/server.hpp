
#include <vector>
#include <thread>
#include <condition_variable>
#include <tuple>

#include <rdmalib/functions.hpp>

namespace server {

  struct Server {
    rdmalib::functions::FunctionsDB db;


  };

  struct Executors {

    std::mutex m;
    std::unique_lock<std::mutex> lk;
    std::vector<std::tuple<rdmalib::functions::FuncType, void*>> _status; 
    std::vector<std::thread> _threads;
    std::condition_variable _cv;

    Executors(int numcores);

    // thread-safe for different ids
    void enable(int thread_id, rdmalib::functions::FuncType func, void* args);
    void disable(int thread_id);
    void wakeup();

    void thread_func(int id);
  };

}

