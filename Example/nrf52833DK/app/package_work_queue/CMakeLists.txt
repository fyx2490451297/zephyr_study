# CMakeLists.txt for package_work_queue
if(CONFIG_PACKAGE_WORK_QUEUE_DEMO)
    target_sources(app PRIVATE src/work_queue_demo.c)
endif()
