cmake_minimum_required (VERSION 2.8)
project (vomid)

set (SOURCES
	src/file.cpp
	src/main.cpp
	src/player.cpp
	src/slot_proxy.cpp
	src/w_file.cpp
	src/w_file_info.cpp
	src/w_main.cpp
	src/w_piano.cpp
	src/w_track.cpp
)

set (MOC_HEADERS
	src/file.h
	src/player.h
	src/w_file.h
	src/w_file_info.h
	src/w_main.h
	src/w_piano.h
	src/w_track.h
)

set (UIS
	src/w_track.ui
	src/w_main.ui
	src/w_file.ui
	src/w_file_info.ui
)

find_package (Qt4 REQUIRED)
include (${QT_USE_FILE})

qt4_wrap_cpp (MOC_SOURCES ${MOC_HEADERS})
qt4_wrap_ui (UI_HEADERS ${UIS})
include_directories (${PROJECT_BINARY_DIR} "libvomid/include")

add_subdirectory (libvomid)
add_executable (vomid WIN32 ${SOURCES} ${MOC_SOURCES} ${UI_HEADERS})
target_link_libraries (vomid libvomid ${QT_LIBRARIES})
