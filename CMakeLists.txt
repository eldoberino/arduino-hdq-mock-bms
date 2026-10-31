cmake_minimum_required(VERSION 3.14)

project(HdqMockBMS
  VERSION 1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(HdqMockBMS_tests
  MockBatteryMgmtSystem.cpp
  tests/MockBatteryMgmtSystemTest.cpp
)

target_include_directories(HdqMockBMS_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(HdqMockBMS_tests
  PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(HdqMockBMS_tests)
