set(
	SOURCES
	CMakeLists.txt
	config.c
	config.h
	event.c
	event.h
	hftirc.c
	hftirc.h
	input.c
	input.h
	irc.c
	irc.h
	parse.c
	parse.h
	parse_api.c
	ui.c
	ui.h
	util.c
	util.h
)

include_directories(
	${CURSES_INCLUDE_DIR}
	${hftirc_BINARY_DIR}
)

add_executable(hftirc ${SOURCES})

# TODO: remove ncursesw code
target_link_libraries(
	hftirc
	ncursesw
)

install(TARGETS hftirc DESTINATION bin)
