#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasInclude.h"

int main() 
{
    Gravitas engine = Gravitas();
    engine.init(800, 800, "Test");
    engine.createEmptyScene();
    engine.run();

    return EXIT_SUCCESS;
}