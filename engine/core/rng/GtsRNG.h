#pragma once

#include <random>

class GtsRNG
{
    private:
        std::mt19937 generator;
        std::uniform_int_distribution<int> distribution;
        std::vector<int> lastFewNumbers;
    public:
        GtsRNG();
        GtsRNG(int lowerbound, int upperbound);
        GtsRNG(int lowerbound, int upperbound, int seed);

        int next();
};