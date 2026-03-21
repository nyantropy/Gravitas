#!/usr/bin/env bash

glslc fragmentshader.frag -o frag.spv
glslc vertexshader.vert -o vert.spv
glslc ui_frag.frag -o ui_frag.spv
glslc ui_vert.vert -o ui_vert.spv
