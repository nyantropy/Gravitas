#!/usr/bin/env bash

glslc fragmentshader.frag -o frag.spv
glslc vertexshader.vert -o vert.spv
glslc text_frag.frag -o text_frag.spv
glslc text_vert.vert -o text_vert.spv
glslc ui_frag.frag -o ui_frag.spv
glslc ui_vert.vert -o ui_vert.spv
