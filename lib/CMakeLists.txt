cmake_minimum_required(VERSION 3.20)
project(AudioVisualizer)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED)
find_package(FFTW3 REQUIRED)

# Add source directory to include paths
include_directories(
    ${SDL3_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    src
)

# Specify the executable path correctly
add_executable(AudioVisualizer src/main.c)

target_link_libraries(AudioVisualizer PRIVATE
    SDL3::SDL3
    ${FFTW3_LIBRARIES}
) 
