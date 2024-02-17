include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../source)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)

set(
    ROOT_SOURCES 
	"renderer.cpp"
	"renderer.hpp"
	"pch.hpp"
	"object.cpp"
	"object.hpp"
	"file_manager.cpp"
	"file_manager.hpp"
	"scope.cpp"
	"scope.hpp"
	"vulkan_api.cpp"
	"vulkan_api.hpp"
	"noise.cpp"
	"noise.hpp"
	"components.hpp"
	"engine.cpp"
	"engine.hpp"
	"window.cpp"
	"window.hpp"
	"event_listener.cpp"
	"event_listener.hpp"
	"core.hpp"
	"enums.hpp"
	"glfw_callbacks.cpp"
	"math.cpp"
	"math.hpp"
)
set(
    VULKAN_SOURCES 
	"vulkan_objects/buffer.cpp"
	"vulkan_objects/buffer.hpp"
	"vulkan_objects/image.cpp"
	"vulkan_objects/image.hpp"
	"vulkan_objects/mesh.cpp"
	"vulkan_objects/mesh.hpp"
	"vulkan_objects/vertex.hpp"
	"vulkan_objects/queue.cpp"
	"vulkan_objects/queue.hpp"
	"vulkan_objects/pipeline.cpp"
	"vulkan_objects/pipeline.hpp"
	"vulkan_objects/descriptor_set.cpp"
	"vulkan_objects/descriptor_set.hpp"
)
set(
    DEBUG_SOURCES 
	"debug_objects/debug_layers.cpp"
)

add_library(source SHARED ${ROOT_SOURCES} ${VULKAN_SOURCES} ${DEBUG_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VULKAN_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DEBUG_SOURCES})

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

	file(GLOB ENGINE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../bin/*)
	add_custom_command(TARGET source POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_SRC} ${COPY_PATH}/)
endif()