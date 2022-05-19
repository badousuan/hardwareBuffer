cmake_minimum_required(VERSION 3.4.1)

add_library(NubiaGLCopy
        SHARED
        GraphicBufferEx.cpp
        GPUIPCommon.cpp
        Utils.cpp
        NubiaGLCopy.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_definitions(-DDEBUG)
add_definitions(-DGL_GLEXT_PROTOTYPES)
add_definitions(-DEGL_EGLEXT_PROTOTYPES)
add_compile_options("-Wno-unused-parameter --allow-shlib-undefined")
#set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --allow-shlib-undefined")
target_link_libraries(NubiaGLCopy
        log
        EGL
        GLESv3
        nativewindow
        android)
