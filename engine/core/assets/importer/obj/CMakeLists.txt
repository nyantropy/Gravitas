# Shared parser implementation; legacy rendering and canonical OBJ import use
# the same TinyOBJ build without linking the importer into production consumers.
add_library(gravitas_tinyobj STATIC TinyObjImplementation.cpp)
target_compile_features(gravitas_tinyobj PRIVATE cxx_std_20)
target_include_directories(gravitas_tinyobj PUBLIC "${GTS_ENGINE_ROOT}/external/stb")

add_library(gravitas_obj_importer STATIC GtsObjModelImporter.cpp ObjSourceReader.cpp)
target_link_libraries(gravitas_obj_importer PUBLIC gravitas_core PRIVATE gravitas_tinyobj)
target_include_directories(gravitas_obj_importer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(gravitas_obj_importer PUBLIC cxx_std_20)
