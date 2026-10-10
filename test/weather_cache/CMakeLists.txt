cmake_minimum_required(VERSION 3.16)
project(WeatherCache LANGUAGES CXX)
enable_testing()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(weather_cache
  WeatherCacheTest.cpp
  ${REPO_ROOT}/lib/Weather/WeatherCache.cpp
)

target_include_directories(weather_cache PRIVATE
  ${REPO_ROOT}/lib/Weather
  ${REPO_ROOT}/test/persistable_store_atomic/stubs
  ${REPO_ROOT}/.pio/libdeps/simulator/ArduinoJson/src
  ${REPO_ROOT}/.pio/libdeps/simulator/simulator/src
  ${REPO_ROOT}/lib/Logging
  ${REPO_ROOT}/include
)

target_compile_features(weather_cache PRIVATE cxx_std_20)
target_compile_options(weather_cache PRIVATE -UNDEBUG)

add_test(NAME weather_cache COMMAND weather_cache)
