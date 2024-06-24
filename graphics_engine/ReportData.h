#pragma once

#include <cstdint>
#include <cstring>

struct ReportData 
{
    double receivedByteRate;
    double packetLossRate;
    double frameRate;

    //serialize the report data
    void serialize(char* buffer)
    {
        std::memcpy(buffer, &receivedByteRate, sizeof(double));
        std::memcpy(buffer + sizeof(double), &packetLossRate, sizeof(double));
        std::memcpy(buffer + 2 * sizeof(double), &frameRate, sizeof(double));  
    }

    //or deserialize it back into the struct
    void deserialize(const char* buffer)
    {
        std::memcpy(&receivedByteRate, buffer, sizeof(double));
        std::memcpy(&packetLossRate, buffer + sizeof(double), sizeof(double));
        std::memcpy(&frameRate, buffer + 2 * sizeof(double), sizeof(double));
    }
};