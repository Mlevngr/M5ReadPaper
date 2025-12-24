cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_definitions(ARDUINO_VARIANT="esp32s3")
project(ReaderPaper)