# vkui is a header-only module — no compilation of its own.
# Its headers are compiled when included by ForwardRenderer (part of the rendering target).
#
# include path layout:
#   ui/           — UICommand.h stub
#   extractors/   — UiCommand.h (UiVertex, UiCommandBuffer, etc.)
#
# Angle-bracket includes in vkui headers bypass the relative-path lookup,
# so #include <UiCommand.h> finds extractors/UiCommand.h via the second path below.
add_library(vkui INTERFACE)

target_include_directories(vkui INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GTS_ENGINE_ROOT}/engine/modules/rendering/extractors"
)
