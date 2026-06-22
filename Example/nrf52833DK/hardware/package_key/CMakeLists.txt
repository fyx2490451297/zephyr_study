# CMakeLists.txt for package_key
if(CONFIG_PACKAGE_KEY)
    zephyr_library_named(package_key)
    zephyr_library_sources(src/key.c)
    zephyr_include_directories(inc)
endif()