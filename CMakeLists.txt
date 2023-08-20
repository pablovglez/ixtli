cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ixtli)
list(APPEND EXTRA_COMPONENT_DIRS components/cam_controller )
