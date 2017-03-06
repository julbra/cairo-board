project(cairo_board C)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})

cmake_minimum_required(VERSION 3.6)

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
find_package(Fontconfig REQUIRED)
include_directories(${FONTCONFIG_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)
pkg_check_modules(GTHREAD REQUIRED gthread-2.0)

include_directories(${GTK_INCLUDE_DIRS})
link_directories(${GTK_LIBRARY_DIRS})
add_definitions(${GTK_CFLAGS_OTHER})

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
        analysis_panel.h
        analysis_panel.c
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
        netstuff.h
        netstuff.c
        san_scanner.h
        san_scanner.c
        test.h
        test.c
        uci-adapter.h
        uci-adapter.c
        uci_scanner.h
        uci_scanner.c)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cairo_board ${SOURCE_FILES})

target_link_libraries(cairo_board ${RSVG_LIBRARIES} ${GTK_LIBRARIES} ${FREETYPE_LIBRARIES} ${FONTCONFIG_LIBRARIES} ${GTHREAD_LIBRARIES} pthread)

