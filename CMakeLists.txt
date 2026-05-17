cmake_minimum_required(VERSION 3.16)

project(Updater LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Подключаем vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Создаём исполняемый файл
add_executable(Updater
    src/main.c
    src/updater.c
    src/updater.h
#    src/ftp_client.cpp
#    src/ftp_client.h
)

# Находим cURL
find_package(CURL CONFIG REQUIRED)

# Линкуем cURL к нашему проекту
target_link_libraries(Updater PRIVATE CURL::libcurl)
