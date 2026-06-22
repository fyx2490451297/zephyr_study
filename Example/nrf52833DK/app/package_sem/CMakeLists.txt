# CMakeLists.txt for package_sem
if(CONFIG_PACKAGE_SEM_DEMO)
    target_sources(app PRIVATE src/sem_demo.c)
endif()
