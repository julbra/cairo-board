project(cairo_board C)

cmake_minimum_required(VERSION 3.6)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)
pkg_check_modules(GTHREAD REQUIRED gthread-2.0)

include_directories(${GTK2_INCLUDE_DIRS})
link_directories(${GTK2_LIBRARY_DIRS})
add_definitions(${GTK2_CFLAGS_OTHER})

include_directories(${RSVG_INCLUDE_DIRS})
link_directories(${RSVG_LIBRARY_DIRS})
add_definitions(${RSVG_CFLAGS_OTHER})

include_directories(${GTHREAD_INCLUDE_DIRS})
link_directories(${GTHREAD_LIBRARY_DIRS})
add_definitions(${GTHREAD_CFLAGS_OTHER})

FIND_PACKAGE(FLEX)
FLEX_TARGET(IcsScanner ics_scanner.lex  ${CMAKE_CURRENT_BINARY_DIR}/ics_scanner.c COMPILE_FLAGS "-Pics_scanner_")
FLEX_TARGET(CraftyScanner crafty_scanner.lex  ${CMAKE_CURRENT_BINARY_DIR}/crafty_scanner.c COMPILE_FLAGS "-Pcrafty_scanner_")
FLEX_TARGET(SanScanner san_scanner.lex  ${CMAKE_CURRENT_BINARY_DIR}/san_scanner.c COMPILE_FLAGS "-Psan_scanner_")
FLEX_TARGET(UciScanner uci_scanner.lex  ${CMAKE_CURRENT_BINARY_DIR}/uci_scanner.c COMPILE_FLAGS "-Puci_scanner_")

set(SOURCE_FILES
        cairo-board.h
        channels.c
        channels.h
        chess-backend.c
        chess-backend.h
        clock-widget.c
        clock-widget.h
        clocks.c
        clocks.h
        configuration.c
        configuration.h
        crafty-adapter.c
        crafty-adapter.h
        crafty_scanner.c
        crafty_scanner.h
        drawing-backend.c
        drawing-backend.h
        ics_scanner.c
        ics_scanner.h
        main.c
        netstuff.c
        netstuff.h
        san_scanner.c
        san_scanner.h
        test.c
        test.h
        uci-adapter.c
        uci_scanner.c
        uci_scanner.h)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cairo_board ${SOURCE_FILES})

target_link_libraries(cairo_board ${GTK2_LIBRARIES} ${RSVG_LIBRARIES} ${GTHREAD_LIBRARIES} pthread)

