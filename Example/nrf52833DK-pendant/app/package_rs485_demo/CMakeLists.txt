# CMakeLists.txt for package_rs485_demo
if(CONFIG_PACKAGE_RS485_DEMO)
    target_sources(app PRIVATE src/rs485_demo.c)
endif()
