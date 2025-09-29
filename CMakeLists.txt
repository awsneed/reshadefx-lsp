cmake_minimum_required(VERSION 3.10.0)
project(reshadefx-lsp-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /bigobj)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wall -Wextra -pedantic)

	if(LSP_USE_SANITIZERS)
		add_compile_options(
			$<$<CONFIG:DEBUG>:-fsanitize=address,undefined>
			$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer>
		)
		add_link_options($<$<CONFIG:DEBUG>:-fsanitize=address,undefined>)
	endif()

	if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-Wa,-mbig-obj)
	endif()
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(deps/lsp-framework)
add_subdirectory(deps/reshade)

add_executable(reshadefx-lsp-server reshadefx-lsp-server.cpp)

target_include_directories(reshadefx-lsp-server
        PRIVATE deps/lsp-framework
        PRIVATE deps/lsp-framework/build/generated
        PRIVATE deps/reshade/source
        PRIVATE /usr/include/spirv/unified1
)

target_link_libraries(reshadefx-lsp-server reshadefx lsp)
