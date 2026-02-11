#include <iostream>
#include <filesystem>
#include <vector>
#include <iostream>

#include "GravitasEngine.hpp"
#include "TetrisScene.hpp"

// creates a simple debug test scene
int main() 
{
    GravitasEngine engine = GravitasEngine();
    engine.registerScene("tetris", std::make_unique<TetrisScene>());
    engine.setActiveScene("tetris");
    engine.start();

    return EXIT_SUCCESS;
}