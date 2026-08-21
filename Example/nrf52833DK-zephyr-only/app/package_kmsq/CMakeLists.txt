# CMakeLists.txt for package_kmsq
if(CONFIG_PACKAGE_KMSQ_DEMO)
    target_sources(app PRIVATE src/kmsq_demo.c)
endif()
