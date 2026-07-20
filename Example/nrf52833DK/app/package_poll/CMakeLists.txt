# CMakeLists.txt for package_poll
if(CONFIG_PACKAGE_POLL_DEMO)
    target_sources(app PRIVATE src/poll_demo.c)
endif()
