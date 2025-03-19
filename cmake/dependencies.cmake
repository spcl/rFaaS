
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

###
# cxxopts
###
find_package(cxxopts QUIET)
if(NOT cxxopts_FOUND)
  message(STATUS "Downloading and building cxxopts dependency")
  FetchContent_Declare(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    CMAKE_ARGS -DCXXOPTS_BUILD_EXAMPLES=Off -DCXXOPTS_BUILD_TESTS=Off
  )
  FetchContent_MakeAvailable(cxxopts)
endif()

###
# spdlog
###
find_package(spdlog 1.8...<1.10 QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "Downloading and building spdlog dependency")
  FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.8.0
  )
  FetchContent_MakeAvailable(spdlog)
else()
  message(STATUS "Using installed spdlog version")
  add_custom_target(spdlog)
endif()

###
# cereal
###
find_package(cereal 1.3.0 EXACT QUIET)
if(NOT cereal_FOUND)
  message(STATUS "Downloading and building cereal dependency")
  FetchContent_Declare(cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    CMAKE_ARGS -DSKIP_PERFORMANCE_COMPARISON=On -DSKIP_PORTABILITY_TEST=On -DJUST_INSTALL_CEREAL=On
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.3.0
  )
  # for some reason the CMAKE_ARGS are ignored here
  set(SKIP_PERFORMANCE_COMPARISON ON CACHE INTERNAL "")
  set(SKIP_PORTABILITY_TEST ON CACHE INTERNAL "")
  set(JUST_INSTALL_CEREAL ON CACHE INTERNAL "")
  FetchContent_MakeAvailable(cereal)
endif()

###
# google test
###
if(${RFAAS_WITH_TESTING})
  include(FetchContent)
  message(STATUS "Downloading and building gtest")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

###
# readerwriterqueue
###
find_package(readerwriterqueue QUIET)
if(NOT readerwriterqueue_FOUND)
  message(STATUS "Downloading and building readerwriterqueue dependency")
  FetchContent_Declare(
    readerwriterqueue
    GIT_REPOSITORY    https://github.com/cameron314/readerwriterqueue
    GIT_TAG           v1.0.5
  )
  FetchContent_MakeAvailable(readerwriterqueue)
endif()

###
# pistache
###
find_package(libpistache 0.4.23 QUIET)
if(NOT libpistache_FOUND)
  message(STATUS "Downloading and building libpistache dependency")
  FetchContent_Declare(
    libpistache
    GIT_REPOSITORY https://github.com/pistacheio/pistache
    GIT_TAG        v0.4.25
  )
  set(PISTACHE_BUILD_TESTS OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(libpistache)
endif()
