#include "RandomNumberGenerator.h"

RandomNumberGenerator::RandomNumberGenerator()
{
    std::random_device rd;
    this->generator = std::mt19937(rd());
    this->distribution = std::uniform_int_distribution<int>(0, 100);
    this->lastFewNumbers.insert(this->lastFewNumbers.begin(), 0);
}

RandomNumberGenerator::RandomNumberGenerator(int lowerbound, int upperbound) : RandomNumberGenerator()
{
    this->distribution = std::uniform_int_distribution<int>(lowerbound, upperbound);
}

RandomNumberGenerator::RandomNumberGenerator(int lowerbound, int upperbound, int seed) : RandomNumberGenerator(lowerbound, upperbound)
{
    this->generator = std::mt19937(seed);
}

int RandomNumberGenerator::next()
{
    int number;
    bool generation = true;

    while(generation)
    {
        number = this->distribution(this->generator);

        for(int i : this->lastFewNumbers)
        {
            if(i != number)
            {
                if(this->lastFewNumbers.size() <= 4)
                {
                    this->lastFewNumbers.insert(this->lastFewNumbers.begin(), number);
                }
                else
                {
                    this->lastFewNumbers.pop_back();
                    this->lastFewNumbers.insert(this->lastFewNumbers.begin(), number);
                }

                generation = false;
            }
        }
    }

    return number;
}