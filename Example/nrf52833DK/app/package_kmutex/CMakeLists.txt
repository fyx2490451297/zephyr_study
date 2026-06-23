# CMakeLists.txt for package_kmutex
if(CONFIG_PACKAGE_KMUTEX_DEMO)
    target_sources(app PRIVATE src/kmutex_demo.c)
endif()
