cmake_minimum_required(VERSION 3.16)
project(agens LANGUAGES CXX)
# Include CTest module for testing functionality
include(CTest)
# Export compile_commands.json for IDEs and linters
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type only for single-config generators; respect user choice and multi-config IDEs
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()
add_library(agens_lib STATIC
  src/system_info.cpp
  src/chat.cpp
  src/backend.cpp
  src/web_search.cpp
  src/file_finder.cpp
  src/agent_mode.cpp
  src/config.cpp
  src/ports.cpp
  src/utils.cpp
)

# Export include directories via the library
target_include_directories(agens_lib PUBLIC
  src
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_include_directories(agens_lib PUBLIC include)
endif()

add_executable(agens
  src/main.cpp
)
target_link_libraries(agens PRIVATE agens_lib)

# Include directories are provided via agens_lib (PUBLIC)

if(APPLE)
  # For popen and sysctl (already in libc), nothing extra.
elseif(UNIX)
  # No extra deps.
endif()
if(BUILD_TESTING)
  add_executable(unit_tests
    tests/test_main.cpp
  )
  target_link_libraries(unit_tests PRIVATE agens_lib)
  add_test(NAME unit COMMAND unit_tests)
  add_test(NAME cli_help COMMAND $<TARGET_FILE:agens> --help)
  add_test(NAME integration COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.sh $<TARGET_FILE:agens>)
  if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(unit_tests PRIVATE -fsanitize=address -fsanitize=undefined)
  endif()
endif()
option(ENABLE_SANITIZERS "Enable AddressSanitizer/UndefinedBehaviorSanitizer" OFF)
# Sanitizers (Clang/GCC only)
if(ENABLE_SANITIZERS)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(SAN_COMPILE_FLAGS -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1)
    target_compile_options(agens_lib PUBLIC ${SAN_COMPILE_FLAGS})
    target_link_options(agens PRIVATE -fsanitize=address -fsanitize=undefined)
  else()
    message(STATUS "Sanitizers requested but not supported by compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()
