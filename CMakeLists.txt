cmake_minimum_required (VERSION 3.0.2)

project (Comet)

 
# add the binary tree to the search path for include files
include_directories(./include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ggdb -std=c++11 -D__CATAPULT__")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)


add_executable(comet.sim 	./src/cache.cpp 
							./src/core.cpp 
							./src/elfFile.cpp 
							./src/memory.cpp 
							./src/multicycleoperator.cpp 
							./src/portability.cpp 
							./src/main.cpp 
							./src/simulator.cpp)

add_executable(atomicTests 	./src/cache.cpp 
							./src/core.cpp 
							./src/atomicTest.cpp
							./src/elfFile.cpp 
							./src/memory.cpp 
							./src/multicycleoperator.cpp 
							./src/portability.cpp
							./src/simulator.cpp)