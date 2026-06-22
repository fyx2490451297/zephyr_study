# CMakeLists.txt for package_led
target_sources(app PRIVATE src/led.c)

# expose the header files of package_led to the whole project
zephyr_include_directories(inc)