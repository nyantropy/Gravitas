#include <iostream>
#include <filesystem>
#include <vector>
#include <iostream>

#include "GravitasEngine.hpp"

int main() 
{
    GravitasEngine engine = GravitasEngine();
    engine.createDebugScene();
    engine.start();

    return EXIT_SUCCESS;
}