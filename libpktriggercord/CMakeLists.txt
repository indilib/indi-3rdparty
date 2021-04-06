cmake_minimum_required(VERSION 3.0.2)
PROJECT(libpktriggercord CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set (PK_VERSION 0.85.01)
set (PK_SOVERSION 0)
set (PK_DATADIR /usr/share/pktriggercord)
set (PK_DEBUG true)
set (PK_WARNINGS true)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

add_definitions(-DPKTDATADIR="${PK_DATADIR}")
add_definitions(-DVERSION="${PK_VERSION}")

#strip main function from pktriggercord
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/pktriggercord-cli.c PK_CLI)
string(REGEX REPLACE "int main.*$" "" PK_CLI "${PK_CLI}")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/libpktriggercord.c "${PK_CLI}")

find_package(INDI REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/src/src/external/js0n)
include_directories( ${INDI_INCLUDE_DIR})

set(libpktriggercord_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pslr.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pslr_model.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pslr_lens.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pslr_enum.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pslr_scsi.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/src/external/js0n/js0n.c
	${CMAKE_CURRENT_SOURCE_DIR}/libpktriggercord.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pktriggercord-servermode.c
)

add_library(pktriggercord SHARED ${libpktriggercord_SRCS})

set_target_properties(pktriggercord PROPERTIES VERSION ${PK_VERSION} SOVERSION ${PK_SOVERSION})

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/src/pentax.rules ${CMAKE_BINARY_DIR}/95-pentax.rules)
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/src/samsung.rules ${CMAKE_BINARY_DIR}/95-samsung.rules)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_BINARY_DIR}/95-pentax.rules ${CMAKE_BINARY_DIR}/95-samsung.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pentax_settings.json DESTINATION ${PK_DATADIR})
endif()

install(TARGETS pktriggercord DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/ DESTINATION include/libpktriggercord FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include/libpktriggercord)


