#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasEngine.hpp"

int main() 
{
    GravitasEngine engine = GravitasEngine();
    engine.createDebugScene();
    engine.start();
    engine.stop();

    return EXIT_SUCCESS;
}