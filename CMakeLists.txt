cmake_minimum_required(VERSION 3.15)
project(samcli VERSION 1.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(samcli
  src/main.c
  src/steam.c
  src/ui.c
  src/platform.c
)

if(WIN32)
  target_link_libraries(samcli PRIVATE advapi32)
else()
  target_link_libraries(samcli PRIVATE ${CMAKE_DL_LIBS})
  # procura a libsteam_api.so do lado do executavel
  set_target_properties(samcli PROPERTIES BUILD_RPATH "\$ORIGIN" INSTALL_RPATH "\$ORIGIN")
endif()

if(MSVC)
  target_compile_options(samcli PRIVATE /W3 /utf-8)
  # CRT estatico: o .exe roda sem precisar do VC++ Redistributable
  set_property(TARGET samcli PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  target_compile_options(samcli PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
