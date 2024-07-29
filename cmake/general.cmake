# Configuration for cmake

include_guard()

# Default build type
if (NOT CMAKE_BUILD_TYPE)
  message(
    STATUS "Setting build type to default 'RelWithDebInfo' ")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build: " FORCE)
  set_property(
    CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
  )
endif()

set(CMAKE_CXX_FLAGS_RELEASE -O2)
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# ccache
option(ENABLE_CACHE "Enable ccache if available" OFF)
if (ENABLE_CACHE)
  find_program(CACHE_BINARY "ccache")
  if (CACHE_BINARY)
    message(STATUS "ccache found and enabled")
    set (CMAKE_CXX_COMPILER_LAUNCHER "ccache")
  else()
    message(WARNING "ccache is enabled but was not found. It will not be used")
  endif()
endif()

# compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_EXPORT_COMPILE_COMMANDS)
  message(STATUS "Exporting compile commands")
else()
  message(STATUS "Not exporting compile commands")
endif()

# sanitizers
function(enable_sanitizers target_name)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(SANITIZERS "")

    string(TOUPPER "ENABLE_SANITIZER_ADDRESS_FOR_${target_name}" ENABLE_SANITIZER_ADDRESS)
    option(${ENABLE_SANITIZER_ADDRESS} "Enable address sanitizer for ${target_name}" FALSE)
    if (${ENABLE_SANITIZER_ADDRESS})
      list(APPEND SANITIZERS "address")
    endif()

    string(TOUPPER "ENABLE_SANITIZER_LEAK_FOR_${target_name}" ENABLE_SANITIZER_LEAK)
    option(${ENABLE_SANITIZER_LEAK} "Enable leak sanitizer for ${target_name}" FALSE)
    if (${ENABLE_SANITIZER_LEAK})
      list(APPEND SANITIZERS "leak")
    endif()

    string(TOUPPER "ENABLE_SANITIZER_UNDEFINED_BEHAVIOR_FOR_${target_name}" ENABLE_SANITIZER_UB)
    option(
      ${ENABLE_SANITIZER_UB} "Enable undefined behavior sanitizer for ${target_name}" FALSE
    )
    if(${ENABLE_SANITIZER_UB})
      list(APPEND SANITIZERS "undefined")
    endif()

    string(TOUPPER "ENABLE_SANITIZER_THREAD_FOR_${target_name}" ENABLE_SANITIZER_THREAD)
    option(${ENABLE_SANITIZER_THREAD} "Enable thread sanitizer for ${target_name}" FALSE)
    if(${ENABLE_SANITIZER_THREAD})
      list(APPEND SANITIZERS "thread")
    endif()

    list(JOIN SANITIZERS "," LIST_OF_SANITIZERS)
  endif()

  if(LIST_OF_SANITIZERS)
    if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
      target_compile_options(${target_name} PRIVATE -fsanitize=${LIST_OF_SANITIZERS})
      target_link_libraries(${target_name} PRIVATE -fsanitize=${LIST_OF_SANITIZERS})
      message("Enabling sanitizers for project '${target_name}': ${LIST_OF_SANITIZERS}")
    endif()
  endif()
endfunction()

# lto
include(CheckIPOSupported)
function(enable_lto target_name)
  string(TOUPPER "ENABLE_LTO_FOR_${target_name}" OPT_NAME)
  option(${OPT_NAME} "Enable link time optimization" OFF)
  if (${OPT_NAME})
    check_ipo_supported(RESULT result OUTPUT output)
    if(result)
      set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
      message(WARNING "LTO is not supported: ${output}")
    endif()
  endif()
endfunction()

# profiling
function(enable_profiling target_name)
    string(TOUPPER "ENABLE_PROFILING_FOR_${target_name}" OPT_NAME)
    option(${OPT_NAME} "Enable profiling" OFF)
    if (${OPT_NAME})
        target_compile_options(${target_name} PRIVATE -pg -g)
        target_link_options(${target_name} PRIVATE -pg)
        message("Enabling profiling for project '${target_name}'")
    endif()
endfunction()

# force colors in compiler output
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
mark_as_advanced(FORCE_COLORED_OUTPUT)
if (${FORCE_COLORED_OUTPUT})
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
  endif()
endif()

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                         Copy compile commands                          #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
# https://stackoverflow.com/a/60910583/7835355
# Copy to source directory
option (COPY_COMPILE_COMMANDS "Forcefully copy compile_commands.json in ./build/" TRUE)
if (${COPY_COMPILE_COMMANDS})
    add_custom_target(
        copy-compile-commands ALL
        DEPENDS
            ${CMAKE_SOURCE_DIR}/build/compile_commands.json
    )
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/build/compile_commands.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/build/compile_commands.json
        DEPENDS
            # Unlike "proper" targets like executables and libraries, 
            # custom command / target pairs will not set up source
            # file dependencies, so we need to list file explicitly here
            generate-compile-commands
            ${CMAKE_BINARY_DIR}/compile_commands.json
    )

    # Generate the compilation commands. Necessary so cmake knows where it came
    # from and if for some reason you delete it.
    add_custom_target(generate-compile-commands
        DEPENDS
            ${CMAKE_BINARY_DIR}/compile_commands.json
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/compile_commands.json
        COMMAND ${CMAKE_COMMAND} -B${CMAKE_BINARY_DIR} -S${CMAKE_SOURCE_DIR}
    )
endif()
