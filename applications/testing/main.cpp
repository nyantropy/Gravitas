#include <iostream>
#include <filesystem>

#include "GravitasInclude.h"
#include "RotationAnimation.hpp"
#include "GtsSceneNodeOpt.h"

const std::string MODEL_PATH = "resources/models/cube.obj";
const std::string TEXTURE_PATH = "resources/textures/blue_texture.png";

Gravitas engine;

void tetrisFrame()
{
    //create a root object at 0/0/0
    GtsSceneNodeOpt obj1;
    //obj1.objectPtr = engine.createObject(MODEL_PATH, "resources/textures/red_texture.png");
    obj1.identifier = "tetrisframeroot";

    engine.addNodeToScene(obj1);

    float lowerboundY = 0.0f;
    float upperboundY = 15.0f;

    float lowerboundX = 0.0f;
    float upperboundX = 11.0f;

    //start at the bottom left column
    for(float x = lowerboundX; x <= upperboundX; x++)
    {
        for(float y = lowerboundY; y <= upperboundY; y++)
        {
            if(y == lowerboundY || x == lowerboundX || x == upperboundX)
            {
                GtsSceneNodeOpt object;
                object.objectPtr = engine.createObject(MODEL_PATH, TEXTURE_PATH);
                object.translationVector = glm::vec3(x - round((upperboundX/2)), -1.0f + y, 0.0f);
                object.parentIdentifier = "tetrisframeroot";

                engine.addNodeToScene(object);
            }
        }
    }
}

int main() 
{
    engine = Gravitas();
    engine.init(600, 800, "Tetris", true);
    engine.createEmptyScene();
    tetrisFrame();
    engine.run();


    return EXIT_SUCCESS;
}