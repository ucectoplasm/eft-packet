cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(EftRadar CXX)

message(STATUS "CMake version: ${CMAKE_VERSION}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (MSVC)
	message(STATUS "MSVC: ${MSVC_VERSION}")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_error "X86 build not supported!")
endif()

if(MSVC)
    set(LIB_RT_SUFFIX "mt")
    set(LIB_RT_OPTION "/MT")

    foreach(flag_var  CMAKE_C_FLAGS  CMAKE_CXX_FLAGS)
         foreach(config_name  ""  DEBUG  RELEASE  MINSIZEREL  RELWITHDEBINFO)
            set(var_name "${flag_var}")

            if(NOT "${config_name}" STREQUAL "")
                set(var_name "${var_name}_${config_name}")
            endif()
            
            string(REPLACE "/MD" "${LIB_RT_OPTION}" ${var_name} "${${var_name}}")
            set(${var_name} "${${var_name}}" CACHE STRING "" FORCE)
        endforeach()
    endforeach()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /NODEFAULTLIB:libci.lib")
    
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(EXTRA_LIBS GlU32d OpenGL32d glew32d json11d minizd pthreadVC2 Packet wpcap Common++d Packet++d Pcap++d SDL2d SDL2maind)
else()
	set(EXTRA_LIBS GlU32 OpenGL32 glew32 json11 miniz pthreadVC2 Packet wpcap Common++ Packet++ Pcap++ SDL2 SDL2main)
endif()

include_directories(${PROJECT_SOURCE_DIR}/extern/include)
include_directories(${PROJECT_SOURCE_DIR}/extern/include/common++)
include_directories(${PROJECT_SOURCE_DIR}/extern/include/packet++)
include_directories(${PROJECT_SOURCE_DIR}/extern/include/pcap++)
link_directories(${PROJECT_SOURCE_DIR}/extern/lib/x64)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../Bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../Bin)

file(GLOB EftRadar_HEADERS
    "src/*.hpp"
)
file(GLOB EftRadar_SOURCES
    "src/*.cpp"
)

add_executable(${PROJECT_NAME} ${EftRadar_HEADERS} ${EftRadar_SOURCES})

target_link_libraries(${PROJECT_NAME} ws2_32 crypt32 Iphlpapi ${EXTRA_LIBS})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_EXTENSIONS OFF)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/extern/bin/x64/pthreadVC2.dll ${PROJECT_SOURCE_DIR}/Bin/pthreadVC2.dll
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/extern/bin/x64/SDL2d.dll ${PROJECT_SOURCE_DIR}/Bin/SDL2.dll
)