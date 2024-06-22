#include <iostream>
#include <filesystem>

#include "GravitasInclude.h"
#include "RotationAnimation.hpp"

const std::string MODEL_PATH = "resources/models/viking_room.obj";
const std::string TEXTURE_PATH = "resources/textures/viking_room.png";

int main() 
{
    Gravitas engine = Gravitas();

    try 
    {
        engine.init(800, 600, "Tetris", true);
        engine.createEmptyScene();
        engine.addNodeToScene(engine.createObject(MODEL_PATH, TEXTURE_PATH), new RotationAnimation());
        engine.run();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}