cmake_minimum_required(VERSION 2.8)
project(hftirc C)

set(CURSES_NEED_NCURSES)
find_package(Curses REQUIRED)

if (CURSES_HAVE_NCURSES_NCURSES_H)
	set(NCURSES_SUBDIRECTORY)
endif()

configure_file(
	${hftirc_SOURCE_DIR}/cmake/port.h.in
	${hftirc_BINARY_DIR}/port.h
)

add_subdirectory(src)
