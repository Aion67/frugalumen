cmake_minimum_required(VERSION 3.16)
# Sandbox project: builds ONE lesson file from main/ against the real components/.
set(EXTRA_COMPONENT_DIRS ../../components)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sandbox_prince_console)
