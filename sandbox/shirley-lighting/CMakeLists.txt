cmake_minimum_required(VERSION 3.16)
# Sandbox project: builds ONE lesson file from main/ against the real components/.
set(EXTRA_COMPONENT_DIRS ../../components)
set(COMPONENTS main)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sandbox_shirley_lighting)
