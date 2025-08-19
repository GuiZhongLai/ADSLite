cmake_minimum_required(VERSION 3.10)
project(ADSLite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  AdsLiteAPI.cpp

  standalone/AdsLiteLib.cpp
  standalone/Sockets.cpp
  standalone/Log.cpp
  standalone/Frame.cpp
  standalone/AmsRouter.cpp
  standalone/AmsConnection.cpp
  standalone/AmsPort.cpp
  standalone/NotificationDispatcher.cpp
)

add_library(ADSLite SHARED ${SOURCES})
if(WIN32)
    target_compile_definitions(ADSLite PRIVATE ADS_LITE_BUILD_DLL)
endif()
set_target_properties(ADSLite PROPERTIES
    SOVERSION ""          
    VERSION ""            
    NO_SONAME FALSE       
    OUTPUT_NAME "ADSLite" 
    DEBUG_POSTFIX ""      
    RELEASE_POSTFIX ""
)
add_library(ADSLite::ADSLite ALIAS ADSLite)

target_include_directories(ADSLite PUBLIC 
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/standalone>
$<INSTALL_INTERFACE:include/ADSLite>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_link_libraries(ADSLite PUBLIC wsock32)
endif()


if(WIN32 EQUAL 1)
    target_link_libraries(ADSLite PUBLIC ws2_32)
endif()

find_package(Threads REQUIRED)
target_link_libraries(ADSLite PUBLIC Threads::Threads)

install(TARGETS ADSLite
    EXPORT ADSLiteConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    AdsLiteDef.h
    AdsLiteAPI.h
    DESTINATION include/ADSLite
)

install(EXPORT ADSLiteConfig
    DESTINATION lib/cmake/ADSLite
    FILE ADSLiteConfig.cmake
)

option(BUILD_EXAMPLES "Build the example program" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
