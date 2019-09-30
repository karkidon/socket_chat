cmake_minimum_required(VERSION 3.13)
project(socket_chat)

set(CMAKE_CXX_STANDARD 11)

add_executable(server socket_chat/server.cpp)
target_link_libraries(server crypto++)

add_executable(client socket_chat/client.cpp)
target_link_libraries(client crypto++)
