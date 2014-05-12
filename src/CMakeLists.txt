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

add_executable(hftirc ${SOURCES})
target_include_directories(hftirc PRIVATE ${CURSES_INCLUDE_DIR} ${hftirc_BINARY_DIR})
target_link_libraries(hftirc ${CURSES_LIBRARIES} ncursesw)		# TODO: remove ncursesw code

install(TARGETS hftirc DESTINATION bin)
