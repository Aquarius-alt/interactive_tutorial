cmake_minimum_required(VERSION 3.16)
project(test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# путь к папке cmake внутри распакованного SDL3 (поправь версию)
set(SDL3_DIR "C:/libs/SDL3-3.4.20/cmake" CACHE PATH "Path to SDL3 cmake config")

find_package(SDL3 REQUIRED CONFIG)

add_executable(test Source.cpp)
target_link_libraries(test PRIVATE SDL3::SDL3)

# копируем SDL3.dll рядом с exe
add_custom_command(TARGET test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_DIR}/../lib/x64/SDL3.dll" $<TARGET_FILE_DIR:test>
)