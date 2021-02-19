cmake_minimum_required(VERSION 3.0)
PROJECT(pigpio LANGUAGES C VERSION 0.71)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

find_package(Threads REQUIRED)
find_package(RT REQUIRED)

option(BUILD_SHARED_LIBS "Create shared libraries" ON)

add_compile_options(-Wall)

# daemon for controlling GPIO
# pigpiod
add_executable(pigpiod pigpiod.c)
target_link_libraries(pigpiod pigpio RT::RT Threads::Threads)

install(TARGETS pigpiod
#install(TARGETS pigpio pigpiod_if2 pigpiod pigs
#    EXPORT ${PROJECT_NAME}Targets
    LIBRARY  DESTINATION lib
    ARCHIVE  DESTINATION lib
    RUNTIME  DESTINATION bin
    INCLUDES DESTINATION include
)

file(GLOB man_1_SRC "*.1")
install(FILES ${man_1_SRC}
    DESTINATION man/man1
    PERMISSIONS OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
)

file(GLOB man_3_SRC "*.3")
install(FILES ${man_3_SRC}
    DESTINATION man/man3
    PERMISSIONS OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
)

# package project
#include (CPack)