
#include <spdlog/spdlog.h>

#include "server.hpp"

namespace server{

  void FunctionsDB::test_function(void* args)
  {
    int val = *static_cast<int*>(args);
    spdlog::debug("Received {}, value {}", fmt::ptr(args), val);
  }

  FunctionsDB::FunctionsDB()
  {
    functions["test"] = test_function;
  }

  Executors::Executors(int numcores):
    lk(m)
  {
    for(int i = 0; i < numcores; ++i) {
      _threads.emplace_back(&Executors::thread_func, this, i);
      _status.emplace_back(nullptr, nullptr);
    }

  }

  void Executors::enable(int thread_id, function_t func, void* args)
  {
    _status[thread_id] = std::make_tuple(func, args);
  }

  void Executors::disable(int thread_id)
  {
    _status[thread_id] = std::make_tuple(nullptr, nullptr);
  }

  void Executors::wakeup()
  {
    _cv.notify_all();
  }

  void Executors::thread_func(int id)
  {
    function_t ptr = nullptr;
    spdlog::debug("Thread {} created!", id);
    while(1) {

      while(!ptr) {
        _cv.wait(lk);
        ptr = std::get<0>(_status[id]);
      }
      void* args = std::get<1>(_status[id]);

      spdlog::debug("Thread {} begins work! Executing function", id);

      (*ptr)(args);
      spdlog::debug("Thread {} finished work!", id);
      this->disable(id);
    } 

  }
}
