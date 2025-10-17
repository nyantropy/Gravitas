add_library(
    attachment

    Attachment.hpp
    Attachment.cpp

    AttachmentConfig.h
)

target_link_libraries(attachment PUBLIC rendering)

target_include_directories(attachment PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")