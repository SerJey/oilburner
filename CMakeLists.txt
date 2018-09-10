cmake_minimum_required(VERSION 3.5)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME oilburner)
project(${PROJECT_NAME})

set(${CMAKE_PROJECT_NAME}_SKETCH oilburner/oilburner.ino)
set(${CMAKE_PROJECT_NAME}_BOARD mega)
set(${CMAKE_PROJECT_NAME}_PORT COM3)

include_directories(${ARDUINO_SDK_PATH}/hardware/arduino/avr/cores/arduino)

generate_arduino_firmware(${CMAKE_PROJECT_NAME})