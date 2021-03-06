cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(XPlaneConnect)

find_package(Freetype REQUIRED)
find_package(SDL2 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(SDK/CHeaders/XPLM)
include_directories(../C/src)

add_definitions(-DXPLM200 -DLIN=1)

SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)
SET(CMAKE_CXX_STANDARD 11)

SET(XPC_OUTPUT_DIR "XPlaneConnect")
SET(XPC_OUTPUT_NAME "lin")

add_library(xpc64 SHARED XPCPlugin.cpp
	DataManager.cpp
	Drawing.cpp
	Log.cpp
	Message.cpp
	MessageHandlers.cpp
	Timer.cpp
	UDPSocket.cpp)

target_link_libraries(xpc64 ${FREETYPE_LIBRARIES})
target_include_directories(xpc64 PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(xpc64 ${SDL2_LIBRARIES})

set_target_properties(xpc64 PROPERTIES PREFIX "" SUFFIX ".xpl")
set_target_properties(xpc64 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${XPC_OUTPUT_DIR}/64)
set_target_properties(xpc64 PROPERTIES OUTPUT_NAME ${XPC_OUTPUT_NAME})
set_target_properties(xpc64 PROPERTIES COMPILE_FLAGS "-m64 -fno-stack-protector" LINK_FLAGS "-shared -rdynamic -nodefaultlibs -undefined_warning -m64 -fno-stack-protector")

add_library(xpc32 SHARED XPCPlugin.cpp
	DataManager.cpp
	Drawing.cpp
	Log.cpp
	Message.cpp
	MessageHandlers.cpp
	Timer.cpp
	UDPSocket.cpp)

# target_link_libraries(xpc32 ${FREETYPE_LIBRARIES})
target_include_directories(xpc32 PRIVATE ${FREETYPE_INCLUDE_DIRS})

# target_link_libraries(xpc32 ${SDL2_LIBRARIES})

set_target_properties(xpc32 PROPERTIES PREFIX "" SUFFIX ".xpl")
set_target_properties(xpc32 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${XPC_OUTPUT_DIR})
set_target_properties(xpc32 PROPERTIES OUTPUT_NAME ${XPC_OUTPUT_NAME})
set_target_properties(xpc32 PROPERTIES COMPILE_FLAGS "-m32 -fno-stack-protector" LINK_FLAGS "-shared -rdynamic -nodefaultlibs -undefined_warning -m32 -fno-stack-protector")
	
# Switch install targets when uncommenting the 32 bit line above.
install(TARGETS xpc64 DESTINATION XPlaneConnect/64 RENAME lin.xpl)
install(TARGETS xpc32 DESTINATION XPlaneConnect/ RENAME lin.xpl)

install(FILES FreeSans.ttf DESTINATION XPlaneConnect/)
install(FILES text.f.glsl DESTINATION XPlaneConnect/)
install(FILES text.v.glsl DESTINATION XPlaneConnect/)

