cmake_minimum_required(VERSION 3.8)
## Modern CMake is only available starting with version 3.0.0.
# alias cmakerelease='cmake $1 -DCMAKE_BUILD_TYPE=RELEASE'

#c++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(DEFAULT_BUILD_TYPE "Release")


project(lab1)


if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to ${DEFAULT_BUILD_TYPE} as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build" FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()
  
if(${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed.${PROJECT_SOURCE_DIR} == ${PROJECT_BINARY_DIR} Please make a new directory (called a build directory) and run CMake from there.")
else()
  message(STATUS "INFORMATION : Out-of-source build")
endif()


message(STATUS "Configuration stages")



add_executable(spy)


# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(spy
  PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(spy
  PRIVATE CMAKE_SPY)
  
target_compile_options(spy
  PUBLIC  -Wall)
  
add_subdirectory(src)
