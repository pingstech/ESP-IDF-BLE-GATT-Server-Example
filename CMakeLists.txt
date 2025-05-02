cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

set(EXTRA_COMPONENT_DIRS src)

message("
==================[ PROJECT BANNER ]==================

██████  ██      ███████     ██████  ███████ ███    ███  ██████  
██   ██ ██      ██          ██   ██ ██      ████  ████ ██    ██ 
██████  ██      █████       ██   ██ █████   ██ ████ ██ ██    ██ 
██   ██ ██      ██          ██   ██ ██      ██  ██  ██ ██    ██ 
██████  ███████ ███████     ██████  ███████ ██      ██  ██████  
                                                                                                                              
=======================================================
")

set(application_name "BLE DEMO")
set(application_version "1.0.1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(main)