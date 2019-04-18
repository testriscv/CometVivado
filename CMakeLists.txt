cmake_minimum_required (VERSION 3.0.2)
project (Comet)

# add the binary tree to the search path for include files
include_directories(./include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -D__CATAPULT__ -O3 -DMEMORY_INTERFACE=CacheMemory ")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)


add_executable(comet.sim 
							./src/core.cpp
							./src/elfFile.cpp
							./src/main.cpp
							./src/simpleMemory.cpp
							./src/riscvISA.cpp
							./src/basic_simulator.cpp)

add_executable(atomicTests
							./src/core.cpp
							./src/atomicTest.cpp
							./src/elfFile.cpp
							./src/simpleMemory.cpp
							./src/riscvISA.cpp)

#target_link_libraries( comet.sim	LINK_PUBLIC ${Boost_LIBRARIES} )
