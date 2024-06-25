#pragma once

#include <cstdint>
#include <cstring>

#include "KeyboardInput.h"

struct KeyboardInputData 
{
    KeyboardInput input;

    // Serialize the KeyboardInput data
    void serialize(char* buffer)
    {
        int32_t inputInt = static_cast<int32_t>(input);
        std::memcpy(buffer, &inputInt, sizeof(int32_t));
    }

    // Deserialize the buffer into the KeyboardInput data
    void deserialize(const char* buffer)
    {
        int32_t inputInt;
        std::memcpy(&inputInt, buffer, sizeof(int32_t));
        input = static_cast<KeyboardInput>(inputInt);
    }
};