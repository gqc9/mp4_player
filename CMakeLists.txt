cmake_minimum_required (VERSION 2.6)
project(player)
include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/lib)
aux_source_directory(. SRC_LIST)
add_executable (player ${SRC_LIST})
target_link_libraries(player
	avcodec
	avdevice
	avfilter
	avformat
	avutil
	postproc
	swresample
	swscale
	SDL2
	SDL2main
	EFX-Util
	OpenAL32
)