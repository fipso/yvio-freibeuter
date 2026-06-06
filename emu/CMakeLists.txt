cmake_minimum_required(VERSION 3.16)
project(yvio_emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -g -O2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UNICORN REQUIRED IMPORTED_TARGET unicorn)
pkg_check_modules(RAYLIB  REQUIRED IMPORTED_TARGET raylib)

set(SRC
  src/main.c
  src/boot.c
  src/cpu_unicorn.c
  src/elf.c
  src/trace.c
  src/sched.c
  src/ipc.c
  src/file.c
  src/syscall.c
  src/audio.c
  src/audio_host.c
  src/rfid.c
  src/ui.c
  src/input.c
  src/server.c
  src/vendor/mongoose.c
)

add_executable(yvio-emu ${SRC})
target_include_directories(yvio-emu PRIVATE src src/vendor)
# Trim mongoose to plain HTTP+WebSocket: no TLS, no directory listing.
set_source_files_properties(src/vendor/mongoose.c PROPERTIES
  COMPILE_DEFINITIONS "MG_TLS=MG_TLS_NONE;MG_ENABLE_DIRLIST=0")
target_link_libraries(yvio-emu PRIVATE PkgConfig::UNICORN PkgConfig::RAYLIB m pthread)
