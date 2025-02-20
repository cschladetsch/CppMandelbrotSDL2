cmake_minimum_required(VERSION 3.15)
project(mandelbrot_explorer
    VERSION 1.0
    DESCRIPTION "Interactive Mandelbrot Set Explorer"
    LANGUAGES CXX
)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# First try to find SDL2 on the system
find_package(SDL2 QUIET)

# If SDL2 is not found, fetch and build it
if(NOT SDL2_FOUND)
    include(FetchContent)
    
    # Configure SDL2 options
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.5  # You can update this to a newer version as needed
    )
    FetchContent_MakeAvailable(SDL2)
endif()

# Create the executable
add_executable(mandelbrot_explorer main.cpp)

# Link against SDL2 and threads
if(SDL2_FOUND)
    target_link_libraries(mandelbrot_explorer PRIVATE 
        SDL2::SDL2
        Threads::Threads
    )
else()
    target_link_libraries(mandelbrot_explorer PRIVATE 
        SDL2::SDL2
        Threads::Threads
    )
endif()

# Set warning levels
if(MSVC)
    target_compile_options(mandelbrot_explorer PRIVATE /W4)
else()
    target_compile_options(mandelbrot_explorer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install configuration
install(TARGETS mandelbrot_explorer
    RUNTIME DESTINATION bin
)
