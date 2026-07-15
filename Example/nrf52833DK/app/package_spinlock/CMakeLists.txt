# CMakeLists.txt for package_spinlock
if(CONFIG_PACKAGE_SPINLOCK_DEMO)
    target_sources(app PRIVATE src/spinlock_demo.c)
endif()
