#pragma once
#ifndef RANDOMNUMBERGENERATOR_H
#define RANDOMNUMBERGENERATOR_H

#include <random>

class RandomNumberGenerator
{
    private:
        std::mt19937 generator;
        std::uniform_int_distribution<int> distribution;
        std::vector<int> lastFewNumbers;
    public:
        RandomNumberGenerator();
        RandomNumberGenerator(int lowerbound, int upperbound);
        RandomNumberGenerator(int lowerbound, int upperbound, int seed);

        int next();
};


#endif //RANDOMNUMBERGENERATOR_H