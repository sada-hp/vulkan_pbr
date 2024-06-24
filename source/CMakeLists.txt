include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../source)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)

file(GLOB ROOT_SOURCES	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB VULKAN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_objects/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_objects/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/renderer_parts/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/renderer_parts/*.hpp")

if (INCLUDE_GUI)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include/imgui)
	file(GLOB IMGUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../include/imgui/*.cpp")
endif()

add_library(source SHARED ${ROOT_SOURCES} ${VULKAN_SOURCES} ${IMGUI_SOURCES})

add_compile_definitions(DEBUG=$<CONFIG:Debug>)
target_compile_definitions(source PRIVATE GRAPI)
set_target_properties(source PROPERTIES  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
set_target_properties(source PROPERTIES  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
target_precompile_headers(source PRIVATE pch.hpp)
target_link_libraries(source assimp.lib glfw3.lib vulkan-1.lib)

if (DEFINED COPY_PATH)
	file(GLOB SHADERS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/*.spv)
	add_custom_command(TARGET source POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${COPY_PATH}/shaders)
	add_custom_command(TARGET source POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_SRC} ${COPY_PATH}/shaders)

	file(GLOB EXTENSIONS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../libraries/*.dll)
	add_custom_command(TARGET source POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${EXTENSIONS_SRC} ${COPY_PATH}/)

	add_custom_command(TARGET source POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:source> ${COPY_PATH})
endif()