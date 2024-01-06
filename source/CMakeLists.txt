include_directories(${DemoPrj_SOURCE_DIR}/include)
include_directories(${DemoPrj_SOURCE_DIR}/source)
link_directories(${DemoPrj_SOURCE_DIR}/libraries)

set(
    ROOT_SOURCES 
	"entry.cpp"
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
add_executable(source ${ROOT_SOURCES} ${VULKAN_SOURCES} ${DEBUG_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VULKAN_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DEBUG_SOURCES})

add_compile_definitions("DEBUG=$<CONFIG:Debug>")
set_target_properties(source PROPERTIES  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DemoPrj_SOURCE_DIR}/bin)
set_target_properties(source PROPERTIES  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DemoPrj_SOURCE_DIR}/bin)
target_precompile_headers(source PRIVATE pch.hpp)
target_link_libraries(source assimp.lib glfw3.lib vulkan-1.lib)


file(GLOB SHADERS_SRC "${DemoPrj_SOURCE_DIR}/shaders/*.spv")
add_custom_command(TARGET source POST_BUILD
		 COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:source>/shaders)
add_custom_command(TARGET source POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS_SRC} $<TARGET_FILE_DIR:source>/shaders/)

file(GLOB CONTENT_SRC "${DemoPrj_SOURCE_DIR}/content/*")
add_custom_command(TARGET source POST_BUILD
		 COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:source>/content)
add_custom_command(TARGET source POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy ${CONTENT_SRC} $<TARGET_FILE_DIR:source>/content/)
		 
file(GLOB EXTENSIONS_SRC "${DemoPrj_SOURCE_DIR}/libraries/*.dll")
add_custom_command(TARGET source POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy ${EXTENSIONS_SRC} $<TARGET_FILE_DIR:source>/)