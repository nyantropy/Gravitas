# renderresource target is defined in backend/vulkan/resources/CMakeLists.txt.
# This directory only contributes IResourceProvider.hpp to the include path.
target_include_directories(renderresource INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")