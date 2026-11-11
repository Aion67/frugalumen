cmake_minimum_required(VERSION 3.16)

# Build only main and what it pulls in. Without this ESP-IDF compiles every
# component it ships with, and its own "console" component clashes with ours.
set(COMPONENTS main)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(frugalumen)
