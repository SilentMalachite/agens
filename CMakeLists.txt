cmake_minimum_required(VERSION 3.16)
project(agens LANGUAGES CXX)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(agens
  src/main.cpp
  src/system_info.cpp
  src/chat.cpp
  src/backend.cpp
  src/web_search.cpp
  src/file_finder.cpp
  src/agent_mode.cpp
  src/config.cpp
)

target_include_directories(agens PRIVATE src)

if(APPLE)
  # For popen and sysctl (already in libc), nothing extra.
elseif(UNIX)
  # No extra deps.
endif()
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
  add_executable(unit_tests
    tests/test_main.cpp
    src/chat.cpp
    src/system_info.cpp
    src/backend.cpp
    src/web_search.cpp
    src/file_finder.cpp
    src/agent_mode.cpp
    src/config.cpp
  )
  target_include_directories(unit_tests PRIVATE src)
  add_test(NAME unit COMMAND unit_tests)
  add_test(NAME cli_help COMMAND agens --help)
endif()
