cmake_minimum_required(VERSION 3.16)
project(CentralizedDelivery CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────────────────────
set(SOURCES
    main.cpp
    src/graph.cpp
    src/dijkstra.cpp
    src/route_optimizer.cpp
    src/order.cpp
    src/order_manager.cpp
    src/rider.cpp
    src/rider_manager.cpp
    src/simulator.cpp
    src/api_server.cpp
)

add_executable(delivery_server ${SOURCES})

# ── Include paths ─────────────────────────────────────────────
target_include_directories(delivery_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor
)

# ── Platform-specific ─────────────────────────────────────────
if(WIN32)
    # Windows: link ws2_32 for socket support (needed by httplib)
    target_link_libraries(delivery_server PRIVATE ws2_32)
    target_compile_definitions(delivery_server PRIVATE
        _WIN32_WINNT=0x0A00
        NOMINMAX
    )
else()
    # Linux/Mac: link pthread
    find_package(Threads REQUIRED)
    target_link_libraries(delivery_server PRIVATE Threads::Threads)
endif()

# ── Compiler warnings ─────────────────────────────────────────
if(MSVC)
    target_compile_options(delivery_server PRIVATE /W3)
else()
    target_compile_options(delivery_server PRIVATE -Wall -Wextra -O2)
endif()

# ── Copy data folder to build directory ───────────────────────
add_custom_command(TARGET delivery_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:delivery_server>/data"
    COMMENT "Copying data folder to build directory"
)

message(STATUS "Build configured. Run: cmake --build . then ./delivery_server")
