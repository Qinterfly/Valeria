
cmake_minimum_required(VERSION 3.22)

# Set parameters

## Data
set(APP_NAME Valeria)
set(APP_CREATOR "Pavel Lakiza")
## Version
set(VERSION_MAJOR 0)
set(VERSION_MINOR 7)
set(VERSION_PATCH 4)
set(VERSION_FULL ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
## Directories
set(INPUT_DIR ${CMAKE_SOURCE_DIR}/input)
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)
set(TEMP_DIR ${CMAKE_BINARY_DIR})
set(RESOURCES_DIR ${CMAKE_SOURCE_DIR}/resources)
set(TRANSLATIONS_DIR ${RESOURCES_DIR}/translations)
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/install)
## Files
set(RESOURCES_FILE ${RESOURCES_DIR}/resources.qrc)
set(INFO_FILE ${RESOURCES_DIR}/info.rc)

# Set language standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE "ON" CACHE BOOL "Force Ninja to use response files")
configure_file(cmake/config.h.in config.h @ONLY)

# Enable caching compiler files
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher" FORCE)
   set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
   if (MSVC)
      foreach(config DEBUG RELWITHDEBINFO)
         foreach(lang C CXX)
            set(flags_var "CMAKE_${lang}_FLAGS_${config}")
            string(REPLACE "/Zi" "/Z7" ${flags_var} "${${flags_var}}")
            set(${flags_var} "${${flags_var}}" PARENT_SCOPE)
         endforeach()
      endforeach()
   endif()
endif()

# Specify project options
project(valeria LANGUAGES CXX)

# Add dependencies
add_subdirectory(lib)

# Add project elements
add_subdirectory(src)
add_subdirectory(tests)
