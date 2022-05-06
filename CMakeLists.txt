cmake_minimum_required (VERSION 3.8)
project ("udp_server_client_filetransfer")
add_executable (udp_server_client_filetransfer "main.cpp")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} pthread)
add_compile_options( -Wsizeof-array-argument )
add_custom_target(run
    COMMAND
	udp_server_client_filetransfer
    VERBATIM
)
