foreach(tool GLSLC SPIRV_VAL SPIRV_DIS)
    if(NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "Missing shader ABI tool ${tool}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(shader skinned_vertexshader.vert vertexshader.vert fragmentshader.frag fragmentshader_pbr.frag)
    execute_process(COMMAND "${GLSLC}" "${SHADER_DIR}/${shader}" -o "${OUTPUT_DIR}/${shader}.spv"
        RESULT_VARIABLE result ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Shader compilation failed: ${errors}")
    endif()
    execute_process(COMMAND "${SPIRV_VAL}" "${OUTPUT_DIR}/${shader}.spv" RESULT_VARIABLE result ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SPIR-V validation failed: ${errors}")
    endif()
endforeach()
execute_process(COMMAND "${SPIRV_DIS}" "${OUTPUT_DIR}/skinned_vertexshader.vert.spv"
    OUTPUT_VARIABLE reflection RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect compiled shader")
endif()
foreach(expected
    "OpDecorate %inPosition Location 0" "OpDecorate %inNormal Location 1"
    "OpDecorate %inTangent Location 2" "OpDecorate %inColor Location 3"
    "OpDecorate %inTexCoord Location 4" "OpDecorate %inJoints Location 5"
    "OpDecorate %inWeights Location 6" "OpDecorate %instanceObjectIndex Location 7"
    "%inJoints = OpVariable %_ptr_Input_v4uint Input"
    "%inWeights = OpVariable %_ptr_Input_v4float Input"
    "%uint = OpTypeInt 32 0" "%v4uint = OpTypeVector %uint 4"
    "OpDecorate %skinPalette DescriptorSet 4" "OpDecorate %skinPalette Binding 0"
    "OpDecorate %_runtimearr_mat4v4float ArrayStride 64"
    "OpMemberDecorate %SkinPalette 0 ColMajor" "OpMemberDecorate %SkinPalette 0 MatrixStride 16"
    "OpMemberDecorate %SkinPalette 0 Offset 0" "OpMemberDecorate %SkinPalette 0 NonWritable")
    string(FIND "${reflection}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Compiled skinned ABI missing ${expected}")
    endif()
endforeach()
execute_process(COMMAND "${SPIRV_DIS}" "${OUTPUT_DIR}/vertexshader.vert.spv" OUTPUT_VARIABLE reflection)
string(FIND "${reflection}" "OpDecorate %instanceObjectIndex Location 5" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Static instance ABI changed")
endif()
