make_minimum_required(VERSION 3.12)

project(rofi)

include(FetchContent)

# Specify common dependencies
FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.11.1
)
FetchContent_GetProperties(catch)
if(NOT catch_POPULATED)
  FetchContent_Populate(catch)
  add_subdirectory(${catch_SOURCE_DIR} ${catch_BINARY_DIR})
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${catch_SOURCE_DIR}/contrib")
  include(Catch)
  include(ParseAndAddCatchTests)
endif()


file(GLOB TEST_SRC *.cpp)
add_executable(test ${TEST_SRC})
target_link_directories(${CMAK})
target_link_libraries(test PRIVATE Catch2::Catch2)