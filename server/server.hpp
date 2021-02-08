
#include <vector>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <tuple>

namespace server {

  typedef void (*function_t)(void*);

  struct FunctionsDB {

    std::unordered_map<std::string, function_t> functions;
    static void test_function(void* args);

    FunctionsDB();
  };

  struct Executors {

    std::mutex m;
    std::unique_lock<std::mutex> lk;
    std::vector<std::tuple<function_t, void*>> _status; 
    std::vector<std::thread> _threads;
    std::condition_variable _cv;

    Executors(int numcores);

    // thread-safe for different ids
    void enable(int thread_id, function_t func, void* args);
    void disable(int thread_id);
    void wakeup();

    void thread_func(int id);
  };

}

