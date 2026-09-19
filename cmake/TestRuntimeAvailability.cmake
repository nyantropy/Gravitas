# An application requiring the real facade succeeds only with the complete composition.
set(source "${TEST_BINARY_ROOT}/runtime-availability")
file(MAKE_DIRECTORY "${source}")
file(WRITE "${source}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.20)\nproject(RuntimeConsumer LANGUAGES C CXX)\n"
    "set(GTS_ENABLE_RENDERING ${RENDERING} CACHE BOOL \"\" FORCE)\n"
    "set(GTS_ENABLE_VULKAN_BACKEND ${VULKAN} CACHE BOOL \"\" FORCE)\n"
    "set(GTS_ENABLE_PHYSICS ${PHYSICS} CACHE BOOL \"\" FORCE)\n"
    "set(GTS_ENABLE_DEBUGDRAW ${DEBUGDRAW} CACHE BOOL \"\" FORCE)\n"
    "set(GTS_ENABLE_TOOLS ${TOOLS} CACHE BOOL \"\" FORCE)\n"
    "add_subdirectory(\"${ENGINE_SOURCE}/engine\" engine)\n"
    "get_target_property(facade_type GravitasEngine TYPE)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${source}/build"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(EXPECT_COMPLETE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Complete facade consumer rejected: ${output}\n${error}")
    endif()
elseif(result EQUAL 0 OR NOT "${output}\n${error}" MATCHES "non-existent target.*GravitasEngine")
    message(FATAL_ERROR "Expected missing GravitasEngine target rejection: ${output}\n${error}")
endif()
