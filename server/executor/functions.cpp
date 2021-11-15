
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>

#include <spdlog/spdlog.h>

#include <rdmalib/util.hpp>
#include "functions.hpp"

// FIXME: works only on Linux
#include <sys/mman.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>

namespace server {

  void extract_symbols(void* library, std::vector<std::string> & names)
  {
    // https://stackoverflow.com/questions/25270275/get-functions-names-in-a-shared-library-programmatically
	  struct link_map * map = nullptr;
		dlinfo(library, RTLD_DI_LINKMAP, &map);

		Elf64_Sym * symtab = nullptr;
		char * strtab = nullptr;
		int symentries = 0;
		for (auto section = map->l_ld; section->d_tag != DT_NULL; ++section)
		{
      if (section->d_tag == DT_SYMTAB)
      {
        symtab = (Elf64_Sym *)section->d_un.d_ptr;
      }
      if (section->d_tag == DT_STRTAB)
      {
        strtab = (char*)section->d_un.d_ptr;
      }
      if (section->d_tag == DT_SYMENT)
      {
        symentries = section->d_un.d_val;
      }
		}
		int size = strtab - (char *)symtab;
		for (int k = 0; k < size / symentries; ++k)
		{
      auto sym = &symtab[k];
      // If sym is function
      if (ELF64_ST_TYPE(symtab[k].st_info) == STT_FUNC)
      {
        //str is name of each symbol
        names.emplace_back(&strtab[sym->st_name]);
      }
		}
    std::sort(names.begin(), names.end());
  }

  Functions::Functions(size_t size):
    _size(size),
    _library_handle(nullptr)
  {
    // FIXME: works only on Linux
    rdmalib::impl::expect_nonnegative(_fd = memfd_create("libfunction", 0));
    rdmalib::impl::expect_zero(ftruncate(_fd, size));

    rdmalib::impl::expect_nonnull(
      _memory_handle = mmap(NULL, size, PROT_WRITE, MAP_SHARED, _fd, 0)
    );
  }

  Functions::~Functions()
  {
    munmap(_memory_handle, _size);
    if(_library_handle)
      dlclose(_library_handle);
  }

  void Functions::process_library()
  {
    //FILE* pFile = fopen("examples/libfunctions.so" , "rb");
    //fseek (pFile , 0 , SEEK_END);
    //size_t len = ftell(pFile);
    //rewind(pFile);
    //fread(_memory_handle,1,len,pFile);
    rdmalib::impl::expect_nonnull(
      _library_handle = dlopen(
        ("/proc/self/fd/" + std::to_string(_fd)).c_str(),
        RTLD_NOW
      ),
      [](){ spdlog::error(dlerror()); }
    );
    extract_symbols(_library_handle, _names);
    _functions.resize(_names.size(), nullptr);
  }

  size_t Functions::size() const
  {
    return _size;
  }

  void* Functions::memory() const
  {
    return this->_memory_handle;
  }

  Functions::FuncType Functions::function(int idx)
  {
    if(!_functions[idx]) {
      _functions[idx] = dlsym(_library_handle, _names[idx].c_str());
    }
    return reinterpret_cast<FuncType>(_functions[idx]);
  }
}

