# Prefer an already-installed package so engine configuration works offline and
# when consumed via add_subdirectory(). Fall back to FetchContent only when no
# system package is available.
find_package(glfw3 CONFIG QUIET)

if(NOT TARGET glfw)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 QUIET glfw3)
    endif()

    if(GLFW3_FOUND)
        add_library(glfw INTERFACE IMPORTED)
        set_target_properties(glfw PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${GLFW3_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIRS}"
            INTERFACE_LINK_DIRECTORIES "${GLFW3_LIBRARY_DIRS}"
            INTERFACE_LINK_LIBRARIES "${GLFW3_LINK_LIBRARIES}"
            INTERFACE_LINK_OPTIONS "${GLFW3_LDFLAGS_OTHER}"
        )
    else()
        include(FetchContent)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
        )
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
    endif()
endif()
