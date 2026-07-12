add_executable(assetc
    assetc.cpp
)

target_compile_features(assetc PRIVATE cxx_std_20)

target_link_libraries(assetc PRIVATE
    gravitas_rendering
)
