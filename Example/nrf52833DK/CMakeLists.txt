cmake_minimum_required(VERSION 3.20.0)

# Set Zephyr SDK path
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Project name
project(nrf52833DK)

# Load hardware-related packages
add_subdirectory(hardware)

# Load top-level application modules
add_subdirectory(app)