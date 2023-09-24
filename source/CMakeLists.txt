include_directories(${DemoPrj_SOURCE_DIR}/include)
link_directories(${DemoPrj_SOURCE_DIR}/libraries)
add_executable(source entry.cpp renderer.cpp renderer.hpp pch.hpp object.cpp object.hpp)
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