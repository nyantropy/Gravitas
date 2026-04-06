file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(external ${CURRENT_DIR_SRC})

target_include_directories(external PUBLIC
    "${GTS_ENGINE_ROOT}/external/stb"
    "${GTS_ENGINE_ROOT}/external/glm/glm"
)
