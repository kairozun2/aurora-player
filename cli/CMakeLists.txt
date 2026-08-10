# Aurora Player - command line player / diagnostics tool (no Qt required).
add_executable(aurora-cli main.cpp)

target_link_libraries(aurora-cli PRIVATE Aurora::Core aurora_flags)

include(GNUInstallDirs)
install(TARGETS aurora-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
