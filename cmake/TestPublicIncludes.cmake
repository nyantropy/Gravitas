set(source "${TEST_BINARY_ROOT}/public-includes-${CASE}")
file(MAKE_DIRECTORY "${source}/core" "${source}/modules/example/contracts" "${source}/runtime")
file(WRITE "${source}/core/CMakeLists.txt" "add_library(foundation INTERFACE)\n")
file(WRITE "${source}/runtime/CMakeLists.txt" "add_library(composition INTERFACE)\n")
set(path "${source}/modules/example/contracts")
if(CASE STREQUAL "module_root")
    set(path "${source}/modules")
elseif(CASE STREQUAL "wrapped_module_root")
    set(path "$<BUILD_INTERFACE:${source}/modules/example/..>")
elseif(CASE STREQUAL "source_root")
    set(path "${source}")
elseif(CASE STREQUAL "ancestor")
    set(path "${source}/modules/example")
elseif(CASE STREQUAL "rendering_root")
    set(path "${source}/modules/rendering")
endif()
file(WRITE "${source}/modules/example/contracts/CMakeLists.txt"
    "add_library(contract INTERFACE)\ntarget_include_directories(contract INTERFACE \"${path}\")\n")
file(WRITE "${source}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.20)\nproject(PublicIncludes NONE)\n"
    "add_subdirectory(core)\nadd_subdirectory(modules/example/contracts)\nadd_subdirectory(runtime)\n"
    "include(\"${ENGINE_SOURCE}/cmake/GravitasSourceLayers.cmake\")\n"
    "include(\"${ENGINE_SOURCE}/cmake/GravitasPublicIncludes.cmake\")\n"
    "set_property(GLOBAL PROPERTY GRAVITAS_SOURCE_LAYER_ROOT \"${source}\")\n"
    "gravitas_check_public_includes()\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${source}/build"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(CASE STREQUAL "allowed")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Allowed leaf export rejected: ${output}\n${error}")
    endif()
elseif(result EQUAL 0 OR NOT "${output}\n${error}" MATCHES "Gravitas public include ownership")
    message(FATAL_ERROR "Expected public include rejection: ${output}\n${error}")
endif()
