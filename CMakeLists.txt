cmake_minimum_required(VERSION 3.10.0)
project(reshade-fx-ls VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(reshade-fx-ls reshade-fx-ls.cpp)

add_subdirectory(deps/lsp-framework)

target_include_directories(reshade-fx-ls
        PUBLIC deps/lsp-framework
        PUBLIC deps/lsp-framework/generated
        PUBLIC deps/reshade/source
)

