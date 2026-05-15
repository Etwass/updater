cmake_minimum_required(VERSION 3.16)
project(Updater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Создаём исполняемый файл
add_executable(Updater
    src/main.cpp
    src/updater.cpp
#    src/updater.h
#    src/ftp_client.cpp
#    src/ftp_client.h
)

# Находим cURL
find_package(CURL CONFIG REQUIRED)

# Линкуем cURL к нашему проекту
target_link_libraries(Updater PRIVATE CURL::libcurl)
