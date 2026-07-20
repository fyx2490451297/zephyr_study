# CMakeLists.txt for package_event
if(CONFIG_PACKAGE_EVENT_DEMO)
    target_sources(app PRIVATE src/event_demo.c)
endif()
