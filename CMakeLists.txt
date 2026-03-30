cmake_minimum_required(VERSION 3.10)
project(ADSLite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  AdsLiteAPI.cpp
  backend/StandaloneBackend.cpp
  backend/BackendSelector.cpp
    backend/BackendServices.cpp
  backend/TwinCATBackend.cpp
  backend/NetworkUtils.cpp
  backend/AdsFileService.cpp
    backend/TargetSystemInfo.cpp

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
if(MSVC)
    target_compile_options(ADSLite PRIVATE /utf-8)
endif()
if(WIN32)
    target_compile_definitions(ADSLite PRIVATE ADS_LITE_BUILD_DLL)

    option(ADSLITE_ENABLE_TWINCAT "Enable TwinCAT backend and link TcAdsDll import library" ON)

    set(TCADSDLL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/TcAdsDll")
    set(TCADSDLL_INCLUDE_DIR "${TCADSDLL_ROOT}/Include")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TCADSDLL_LIB_DIR "${TCADSDLL_ROOT}/x64/lib")
        set(TCADSDLL_DLL "${TCADSDLL_ROOT}/x64/TcAdsDll.dll")
    else()
        set(TCADSDLL_LIB_DIR "${TCADSDLL_ROOT}/Lib")
    endif()

    set(TCADSDLL_LIB_FILE "${TCADSDLL_LIB_DIR}/TcAdsDll.lib")
    if(ADSLITE_ENABLE_TWINCAT AND EXISTS "${TCADSDLL_INCLUDE_DIR}/TcAdsAPI.h" AND EXISTS "${TCADSDLL_LIB_FILE}")
        target_compile_definitions(ADSLite PRIVATE ADSLITE_TWINCAT_ENABLED=1)
        target_include_directories(ADSLite PRIVATE "${TCADSDLL_INCLUDE_DIR}")
        target_link_libraries(ADSLite PRIVATE "${TCADSDLL_LIB_FILE}")
        message(STATUS "ADSLite: TwinCAT backend enabled, linking ${TCADSDLL_LIB_FILE}")
    else()
        target_compile_definitions(ADSLite PRIVATE ADSLITE_TWINCAT_ENABLED=0)
        message(STATUS "ADSLite: TwinCAT backend disabled (missing TcAdsDll headers/libs or ADSLITE_ENABLE_TWINCAT=OFF)")
    endif()
endif()
set_target_properties(ADSLite PROPERTIES
    SOVERSION "1"          
    VERSION "1.0.0"            
    NO_SONAME FALSE       
    OUTPUT_NAME "ADSLite" 
    DEBUG_POSTFIX ""      
    RELEASE_POSTFIX ""
)
add_library(ADSLite::ADSLite ALIAS ADSLite)

target_include_directories(ADSLite PUBLIC 
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backend>
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/standalone>
$<INSTALL_INTERFACE:include/ADSLite>
)

if(WIN32)
    target_link_libraries(ADSLite PUBLIC ws2_32 iphlpapi)
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
    if(MSVC)
        target_compile_options(example PRIVATE /utf-8)
        target_compile_options(example_file PRIVATE /utf-8)
    endif()
endif()
