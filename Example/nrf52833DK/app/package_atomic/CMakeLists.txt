# CMakeLists.txt for package_atomic
if(CONFIG_PACKAGE_ATOMIC_DEMO)
    target_sources(app PRIVATE src/atomic_demo.c)
endif()
