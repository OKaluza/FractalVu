################################################################
# FractalViewer Omegalib build
#---------------------------------------------------------------
# Copyright 2016
# Authors:                    
#  Owen Kaluza              owen@ozone.id.au
################################################################
cmake_minimum_required(VERSION 2.8.1) 
project(FractalVu)

#request_dependency(omegalib)
cmake_minimum_required(VERSION 2.8.1) 
find_package(Omegalib)

set (CMAKE_CXX_STANDARD 11)

#LavaVu 
set(LAVAVU_INCLUDE "../../../LavaVu/src" CACHE FILEPATH "Where to find LavaVu headers")
set(LAVAVU_LIBPATH "../" CACHE FILEPATH "Where to find LavaVu library")

# Source files
SET( srcs 
  FractalVR.cpp
        ../src/FractalVu.cpp
        ../src/FractalServer.cpp
        )
    
######################################################################################
# Headers
SET( headers 
        ../src/FractalVu.h
        ../src/FractalServer.h
        ../src/FractalUtil.h
        ../src/MidiInput.h
        ) 
        
######################################################################################
# Setup compile info

#add_definitions(-DDEBUG -g)
add_definitions(-O3)
add_definitions(-Wno-deprecated-declarations)

include_directories(${OMEGA_INCLUDE_DIRS} ${LAVAVU_INCLUDE})

link_directories(${LAVAVU_LIBPATH} )

add_executable(FractalVR ${srcs} ${headers})

target_link_libraries(FractalVR m dl stdc++ python2.7 LavaVu ${OMEGA_LIB} ${OMEGA_TOOLKIT_LIB})

