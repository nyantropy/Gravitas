#include <iostream>
#include <filesystem>
#include <vector>
#include <iostream>

#include "GravitasEngine.hpp"

// creates a simple debug test scene
int main() 
{
    GravitasEngine engine = GravitasEngine();
    engine.createDebugScene();
    engine.start();

    return EXIT_SUCCESS;
}