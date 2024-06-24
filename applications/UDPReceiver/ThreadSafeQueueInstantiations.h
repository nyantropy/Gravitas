#pragma once

#include "ThreadSafeQueue.h"
#include "FrameData.h"

//this is needed for the linker to not go bonkers, i guess generics are a bit more specific in c++

template class ThreadSafeQueue<FrameData>;