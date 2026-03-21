# vkui is a header-only module — no compilation of its own.
# Its headers are compiled when included by ForwardRenderer (part of the rendering target).
#
# include path layout:
#   ui/           — VulkanUiRenderer.hpp, UIGlyphVertexDescription.h (empty UICommand.h stub)
#   extractors/   — the canonical UICommand.h (UIGlyphVertex, UICommandList)
#
# Angle-bracket includes in vkui headers bypass the relative-path lookup,
# so #include <UICommand.h> finds extractors/UICommand.h via the second path below.
add_library(vkui INTERFACE)

target_include_directories(vkui INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/engine/modules/rendering/extractors"
)
