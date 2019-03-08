cmake_minimum_required(VERSION 3.6)
project(live5555)

set(CMAKE_CXX_STANDARD 11)

set(LIVE555_HOME /home/alex/CPP/lib/live555)

add_definitions(-D_WEBSOCKETPP_CPP11_FUNCTIONAL_)
add_definitions(-D_WEBSOCKETPP_CPP11_THREAD_)
add_definitions(-D_WEBSOCKETPP_CPP11_SYSTEM_ERROR_)
add_definitions(-D_WEBSOCKETPP_CPP11_MEMORY_)
add_definitions(-D_REENTRANT)


include_directories(${LIVE555_HOME}/include/UsageEnvironment)
include_directories(${LIVE555_HOME}/include/BasicUsageEnvironment)
include_directories(${LIVE555_HOME}/include/liveMedia)
include_directories(${LIVE555_HOME}/include/groupsock)

include_directories(/home/alex/CPP/lib/spdlog/include)

include_directories(/home/alex/CPP/lib/websocketpp/include)
include_directories(/home/alex/CPP/lib/boost/1.65.1/include)


set(CMAKE_CXX_FLAGS "-w -pthread")

set(LIVE5555_SRC SinkBase.cpp RTSPClientBase.cpp)

add_library(live5555 ${LIVE5555_SRC})
target_link_libraries(
        live5555
        ${LIVE555_HOME}/lib/libliveMedia.a
        ${LIVE555_HOME}/lib/libgroupsock.a
        ${LIVE555_HOME}/lib/libBasicUsageEnvironment.a
        ${LIVE555_HOME}/lib/libUsageEnvironment.a
)

set(WS_PUSH_SRC wspush.cpp WebSocketClient.cpp WebSocketClient.cpp)
add_executable(wspush ${WS_PUSH_SRC})
target_link_libraries(
        wspush
        live5555
        /home/alex/CPP/lib/boost/1.65.1/lib/libboost_system.so
)