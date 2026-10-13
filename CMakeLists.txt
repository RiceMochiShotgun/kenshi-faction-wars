# Builds the simulation core, its tests and the headless runner with a modern
# compiler. The in-game plugin is built separately with Visual C++ 2010 (v100).
cmake_minimum_required(VERSION 3.10)
project(FactionWars CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The core must compile with Visual C++ 2010, so hold it to C++03 here and
# let the compiler flag anything newer.
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -pedantic -Wno-long-long)
  option(FW_WERROR "Treat warnings as errors" ON)
  if(FW_WERROR)
    add_compile_options(-Werror)
  endif()
endif()

file(GLOB FW_CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/src/*.cpp)
add_library(fw_core STATIC ${FW_CORE_SOURCES})
target_include_directories(fw_core PUBLIC ${CMAKE_SOURCE_DIR}/core/include)
target_include_directories(fw_core SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/rapidjson/include)

file(GLOB FW_TEST_SUPPORT ${CMAKE_SOURCE_DIR}/tests/support/*.cpp)
add_library(fw_test_support STATIC ${FW_TEST_SUPPORT})
target_include_directories(fw_test_support PUBLIC ${CMAKE_SOURCE_DIR}/tests/support)
target_link_libraries(fw_test_support PUBLIC fw_core)

file(GLOB FW_TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/unit/*.cpp)
add_executable(fw_tests ${FW_TEST_SOURCES})
target_link_libraries(fw_tests PRIVATE fw_test_support)
target_compile_definitions(fw_tests PRIVATE FW_DATA_DIR="${CMAKE_SOURCE_DIR}/data/FactionWars")

add_executable(fw_headless ${CMAKE_SOURCE_DIR}/tests/headless/main.cpp)
target_link_libraries(fw_headless PRIVATE fw_test_support)
target_compile_definitions(fw_headless PRIVATE FW_DATA_DIR="${CMAKE_SOURCE_DIR}/data/FactionWars")

enable_testing()
add_test(NAME unit COMMAND fw_tests)
add_test(NAME headless_10_years COMMAND fw_headless --years 10 --quiet)
