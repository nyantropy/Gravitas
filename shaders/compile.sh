#!/usr/bin/env bash

glslc fragmentshader.frag -o frag.spv
glslc fragmentshader_pbr.frag -o frag_pbr.spv
glslc vertexshader.vert -o vert.spv
glslc particle_frag.frag -o particle_frag.spv
glslc particle_vert.vert -o particle_vert.spv
glslc particle_mesh_frag.frag -o particle_mesh_frag.spv
glslc particle_mesh_vert.vert -o particle_mesh_vert.spv
glslc ui_frag.frag -o ui_frag.spv
glslc ui_vert.vert -o ui_vert.spv
glslc upscale_frag.frag -o upscale_frag.spv
glslc upscale_vert.vert -o upscale_vert.spv
