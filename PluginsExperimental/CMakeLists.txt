cmake_minimum_required(VERSION 3.31)


file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

# Loops every subfolder and compiles separate plugins 
# Check examples/Plugins/TemplatePlugin for plugin template
foreach(subdir ${children})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${subdir} AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt")
        add_subdirectory(${subdir})
    endif()
endforeach()   