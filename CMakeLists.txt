cmake_minimum_required(VERSION 3.20)
project(Axolotl)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

# Add the executable
add_executable(axolotl 
                        main.cpp
                        src/Value.cpp
)

target_include_directories(axolotl PRIVATE include)

target_compile_features(axolotl PUBLIC cxx_std_23)

