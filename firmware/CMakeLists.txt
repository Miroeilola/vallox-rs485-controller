# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)

# Firmware version comes from the git tag, never from a hand-maintained constant.
execute_process(
  COMMAND git describe --tags --dirty --always
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE FW_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT FW_VERSION)
  set(FW_VERSION "unknown")
endif()
add_compile_definitions(FW_VERSION="${FW_VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vallox_rs485_controller)
