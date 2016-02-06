####################################################################################################################### 
# FractalViewer Omegalib build
#---------------------------------------------------------------------------------------------------------------------
# Copyright 2016
# Authors:                    
#  Owen Kaluza              owen@ozone.id.au
#######################################################################################################################
cmake_minimum_required(VERSION 2.8.1) 
project(FractalViewer)

find_package(Omegalib)

#LavaVu 
set(LAVAVU_INCLUDE "../../LavaVu/src" CACHE FILEPATH "Where to find LavaVu headers")
set(LAVAVU_LIB "../../LavaVu/bin" CACHE FILEPATH "Where to find LavaVu library")

# Source files
SET( srcs 
        Fractals.cpp
        ../src/FractalVu.cpp
        ../src/FractalServer.cpp
        )
    
#######################################################################################################################
# Headers
SET( headers 
        ../src/FractalVu.h
        ../src/FractalServer.h
        ../src/FractalUtil.h
        ../src/MidiInput.h
        ) 
        
#######################################################################################################################
# Setup compile info

add_definitions(-DUSE_OMEGALIB)
#add_definitions(-DUSE_OMEGALIB -DDEBUG -g)
add_definitions(-O3)

include_directories(${OMEGA_INCLUDE_DIRS} ${LAVAVU_INCLUDE})

add_executable(FractalViewer ${srcs} ${headers})

target_link_libraries(FractalViewer dl ${LAVAVU_LIB} ${OMEGA_LIB} ${OMEGA_TOOLKIT_LIB})

