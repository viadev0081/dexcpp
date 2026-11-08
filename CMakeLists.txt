cmake_minimum_required(VERSION 3.16)
project(DexCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer the wide-character ncurses build so UTF-8 input/output works.
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CURSES_INCLUDE_DIRS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _XOPEN_SOURCE=600
    _DEFAULT_SOURCE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CURSES_LIBRARIES}
)

