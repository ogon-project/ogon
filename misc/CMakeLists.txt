configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-rdp-server.service.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-rdp-server.service)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-session-manager.service.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-session-manager.service)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-session-manager.init.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-session-manager.init)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-rdp-server.init.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-rdp-server.init)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.ini.debian.in ${CMAKE_CURRENT_BINARY_DIR}/config.ini.debian)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-start-vc.sh.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-start-vc.sh)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/ogon-start-vc.sh" DESTINATION ${CMAKE_INSTALL_BINDIR})

install(PROGRAMS "ogonXsession" DESTINATION ${OGON_CFG_PATH})
install(PROGRAMS "ogon_cleanup.sh" DESTINATION ${OGON_CFG_PATH})

if(WITH_OPENH264)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-get-openh264-codec.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-get-openh264-codec)
	install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/ogon-get-openh264-codec" DESTINATION ${CMAKE_INSTALL_SBINDIR})
endif()
