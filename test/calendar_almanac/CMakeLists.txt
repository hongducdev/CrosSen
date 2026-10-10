cmake_minimum_required(VERSION 3.16)
project(CalendarAlmanac LANGUAGES CXX)
enable_testing()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(calendar_almanac
  CalendarAlmanacTest.cpp
  ${REPO_ROOT}/src/components/calendar/CalendarAlmanac.cpp
  ${REPO_ROOT}/lib/LunarCalendar/LunarCalendar.cpp
)

target_include_directories(calendar_almanac PRIVATE
  ${REPO_ROOT}/src/components/calendar
  ${REPO_ROOT}/lib/LunarCalendar
)

target_compile_features(calendar_almanac PRIVATE cxx_std_20)
target_compile_options(calendar_almanac PRIVATE -Wall -Wextra -pedantic -UNDEBUG)

add_test(NAME calendar_almanac COMMAND calendar_almanac)
