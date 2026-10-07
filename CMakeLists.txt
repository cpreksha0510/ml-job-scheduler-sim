cmake_minimum_required(VERSION 3.16)
project(ml_job_scheduler_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings (satisfies AGENT_RULES: -Wall -Wextra)
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# -- include paths ------------------------------------------------------------
include_directories(include)

# -- library: core (Job, Scheduler interface, Simulator) ----------------------
add_library(scheduler_core STATIC
    src/simulator.cpp
)

# -- main executable ----------------------------------------------------------
add_executable(scheduler_sim
    src/main.cpp
)
target_link_libraries(scheduler_sim PRIVATE scheduler_core)

# -- tests (CTest hook for future use) ----------------------------------------
enable_testing()
