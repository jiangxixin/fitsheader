cmake_minimum_required(VERSION 3.16)
project(fitsheader VERSION 1.0.0 LANGUAGES C)

# Require C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Show full compiler output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Build type: RelWithDebInfo by default, allow override
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# ---- Find dependencies ----
find_package(Threads REQUIRED)
find_package(cfitsio REQUIRED)

# ---- Binary ----
add_executable(fitsheader
    src/main.c
)

target_link_libraries(fitsheader PRIVATE CFITSIO::cfitsio)



# ---- Compiler warnings ----
if(MSVC)
    target_compile_options(fitsheader PRIVATE /W4)
else()
    target_compile_options(fitsheader PRIVATE
        -Wall -Wextra -Wpedantic
        -O3
    )
endif()

# ---- Install ----
install(TARGETS fitsheader DESTINATION bin)
