cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(ReversedEchoServer)

add_executable(ReversedEchoServer ./server.cpp)

find_package(Poco COMPONENTS Net Util JSON REQUIRED)

target_link_libraries(ReversedEchoServer PUBLIC Poco::Net Poco::Util Poco::JSON)
