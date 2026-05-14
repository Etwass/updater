cmake_minimum_required(VERSION 3.16)
project(Updater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Updater
    src/main.cpp
    src/updater.cpp
#    src/updater.h
#    src/ftp_client.cpp
#    src/ftp_client.h
)
