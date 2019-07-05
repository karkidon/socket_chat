cmake_minimum_required(VERSION 3.14)
project(socket_chat)

set(CMAKE_CXX_STANDARD 14)

add_executable(server server.cpp)
target_link_libraries(server crypto++)
add_executable(client client.cpp)
target_link_libraries(client crypto++)
