cmake_minimum_required(VERSION 3.16)
project(tx_tool CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- Dependencies ----------------

find_package(OpenSSL REQUIRED)

# secp256k1
find_library(SECP256K1_LIB secp256k1 REQUIRED)
find_path(SECP256K1_INCLUDE_DIR secp256k1.h REQUIRED)

# nlohmann/json
find_package(nlohmann_json 3.10 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found locally – fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ---------------- Sources ----------------

set(SOURCES
    main.cpp
    transaction.cpp
    accounting.cpp
    json_helper.cpp
    script.cpp
    script_processor.cpp
    utilities.cpp
    block.cpp
    block_parser.cpp
    external/bech32.c
    external/libbase58.c
)

# ---------------- Target ----------------

add_executable(tx_tool ${SOURCES})

# Includes
target_include_directories(tx_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${SECP256K1_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(tx_tool PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${SECP256K1_LIB}
)

# ---------------- Compiler Warnings ----------------

target_compile_options(tx_tool PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
    >
)

# ---------------- Build Type ----------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenSSL    : ${OPENSSL_VERSION}")