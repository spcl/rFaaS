
#ifndef __SERVER_FUNCTIONS_HPP__
#define __SERVER_FUNCTIONS_HPP__

#include <vector>
#include <string>

#include <rdmalib/buffer.hpp>

namespace server {

  void extract_symbols(void* handle, std::vector<std::string> & names);

  struct Functions
  {
    int _fd;
    void* _memory_handle;
    size_t _size;
    void* _library_handle;
    // FIXME: small vector?
    std::vector<std::string> _names;
    std::vector<void*> _functions;

    typedef uint32_t (*FuncType)(void*, uint32_t, void*);

    Functions(size_t size);
    ~Functions();

    void process_library();
    size_t size() const;
    void* memory() const;
    FuncType function(int idx);
  };

}

#endif
