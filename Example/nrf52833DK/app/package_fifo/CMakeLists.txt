# CMakeLists.txt for package_fifo
if(CONFIG_PACKAGE_FIFO_DEMO)
    target_sources(app PRIVATE src/fifo_demo.c)
endif()
