cmake_minimum_required(VERSION 3.16)
project(FIRDigitalFilter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fir_core
    src/fir_filter.cpp
    src/filter_design.cpp
    src/signal_generator.cpp
)
target_include_directories(fir_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(fir_engine src/main.cpp)
target_link_libraries(fir_engine PRIVATE fir_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_fir tests/test_fir.cpp)
    target_link_libraries(test_fir PRIVATE fir_core)
    add_test(NAME fir_tests COMMAND test_fir)
endif()

if(MSVC)
    target_compile_options(fir_core PRIVATE /W4)
    target_compile_options(fir_engine PRIVATE /W4)
    if(BUILD_TESTING)
        target_compile_options(test_fir PRIVATE /W4)
    endif()
else()
    target_compile_options(fir_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(fir_engine PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTING)
        target_compile_options(test_fir PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
