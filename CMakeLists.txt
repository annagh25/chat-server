cmake_minimum_required(VERSION 3.11)

project(Radio CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Ищем Boost версии 1.78
find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Под Windows нужно определить макрос NOMINMAX для корректной работы при включении
# библиотеки minisound
if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

add_executable(client client/client.cpp)
add_executable(server server/server.cpp
        server/Session.cpp
        server/Session.h
        server/Room.cpp
        server/Room.h)
target_link_libraries(client PRIVATE Threads::Threads dl)
target_link_libraries(server PRIVATE Threads::Threads dl)

