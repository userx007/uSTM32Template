cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr 
	REQUIRED 
	HINTS 
		$ENV{ZEPHYR_BASE}
)

project(Zephyr_Shell)

add_subdirectory(libs)

target_sources(app 
	PRIVATE 
		src/main.c
)