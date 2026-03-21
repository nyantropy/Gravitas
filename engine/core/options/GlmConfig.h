#pragma once

// Central GLM configuration — include this instead of including GLM directly.
// Defines must appear before any GLM header is included. By routing all GLM
// includes through this file, the defines are guaranteed to be in effect for
// every translation unit.

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/hash.hpp>
