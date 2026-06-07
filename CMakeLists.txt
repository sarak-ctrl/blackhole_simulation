cmake_minimum_required(VERSION 3.16)
project(BlackHoleSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(GLEW   REQUIRED)
find_package(glm    REQUIRED)

add_executable(black_hole black_hole.cpp)

target_include_directories(black_hole PRIVATE
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

target_link_libraries(black_hole PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)

if(APPLE)
    target_compile_definitions(black_hole PRIVATE GL_SILENCE_DEPRECATION)
endif()

set(SHADERS
    bh.vert bh.frag
    grid.vert grid.frag
    blur.frag composite.frag
)

foreach(SHADER ${SHADERS})
    configure_file(${CMAKE_SOURCE_DIR}/${SHADER}
                   ${CMAKE_BINARY_DIR}/${SHADER} COPYONLY)
endforeach()