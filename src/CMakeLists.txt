# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources}
	REQUIRES filesystem api_server_controller cam_controller driver esp_timer esp32-camera mic_controller
		stream_controller ethernet_component
	PRIV_REQUIRES nvs_flash esp_https_server esp_partition esp_https_ota app_update 
	)
