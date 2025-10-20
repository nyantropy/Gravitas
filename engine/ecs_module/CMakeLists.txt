add_library(
    ecs

    ComponentStorage.hpp
    ComponentStorage.cpp

    ECSWorld.hpp
    ECSWorld.cpp

    Entity.h

    System.cpp
    System.hpp
)

target_include_directories(ecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
#target_link_libraries(ecs PUBLIC instance surface logicaldevice physicaldevice swapchain)