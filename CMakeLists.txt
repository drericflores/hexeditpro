cmake_minimum_required(VERSION 3.16)
project(efhex VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(EFHEX_BUILD_GUI "Build the GTK application (needs gtkmm-3.0)" ON)
option(EFHEX_BUILD_TESTS "Build the unit tests" ON)

add_library(hexcore STATIC
  src/HexBuffer.cpp
  src/HexUtil.cpp)
target_include_directories(hexcore PUBLIC include)
target_compile_options(hexcore PRIVATE -Wall -Wextra -Wpedantic)

if(EFHEX_BUILD_GUI)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)

  add_executable(efhex
    src/main.cpp
    src/MainWindow.cpp
    src/HexView.cpp
    src/menu.cpp)
  target_link_libraries(efhex PRIVATE hexcore PkgConfig::GTKMM)
  target_compile_definitions(efhex PRIVATE EFHEX_VERSION="${PROJECT_VERSION}")
  target_compile_options(efhex PRIVATE -Wall -Wextra)

  install(TARGETS efhex RUNTIME DESTINATION bin)

  # Launcher entry and icon so efhex shows up in the GNOME menu.
  install(FILES resources/com.eric.efhex.desktop
          DESTINATION share/applications)
  if(EXISTS "${CMAKE_SOURCE_DIR}/resources/efhex.png")
    install(FILES resources/efhex.png
            DESTINATION share/icons/hicolor/256x256/apps)
  else()
    message(WARNING "resources/efhex.png not found: efhex will install without an icon")
  endif()
endif()

if(EFHEX_BUILD_TESTS)
  enable_testing()
  add_executable(test_core tests/test_core.cpp)
  target_link_libraries(test_core PRIVATE hexcore)
  target_compile_options(test_core PRIVATE -Wall -Wextra)
  add_test(NAME core COMMAND test_core)
endif()
