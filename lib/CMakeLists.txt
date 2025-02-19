cmake_minimum_required(VERSION 3.20)
project(AudioVisualizer)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED)
find_package(FFTW3 REQUIRED)

include_directories(
    ${SDL3_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

add_executable(main src/main.c)

target_link_libraries(main
    ${SDL3_LIBRARIES}
    ${FFTW3_LIBRARIES}
)