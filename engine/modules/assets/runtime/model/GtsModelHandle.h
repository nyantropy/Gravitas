#pragma once

#include <memory>

class GtsModelResource;

// shared immutable resource reference - the registry retains every successful load
using GtsModelHandle = std::shared_ptr<const GtsModelResource>;
