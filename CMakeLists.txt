cmake_minimum_required (VERSION 3.0.2)
project (Comet)

find_package(Boost COMPONENTS program_options REQUIRED)
# add the binary tree to the search path for include files
include_directories(./include
				/usr/include/boost
		    ${Boost_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lboost_program_options -g -std=c++11 -D__CATAPULT__ -O3 -DMEMORY_INTERFACE=CacheMemory ")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)


add_executable(comet.sim 
							./src/core.cpp
							./src/elfFile.cpp
							./src/main.cpp
							./src/memory.cpp
							./src/simpleMemory.cpp
							./src/riscvISA.cpp
							./src/basic_simulator.cpp)

add_executable(atomicTests
							./src/core.cpp
							./src/atomicTest.cpp
							./src/elfFile.cpp
							./src/simpleMemory.cpp
							./src/riscvISA.cpp)

target_link_libraries( comet.sim	LINK_PUBLIC ${Boost_LIBRARIES} )
