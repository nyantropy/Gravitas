#include <iostream>
#include <filesystem>

#include "GravitasInclude.h"
#include "RotationAnimation.hpp"
#include "GtsSceneNodeOpt.h"
#include "SlowFallAnimation.hpp"

const glm::vec3 right_rotation = glm::vec3(0.0f, 0.0f, -90.0f);
const glm::vec3 left_rotation = glm::vec3(0.0f, 0.0f, 90.0f);

const std::string MODEL_PATH = "resources/models/cube.obj";
const std::string FRAME_TEXTURE_PATH = "resources/textures/grey_texture.png";
const std::string I_PIECE_TEXTURE_PATH = "resources/textures/blue_texture.png";

std::vector<std::vector<int>> tetrisGrid;
Gravitas engine;

void onKeyPressed(int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        return;
    }

    //hardcode this to glfw keys for now
    switch(key)
    {
        case GLFW_KEY_LEFT:
            engine.getSelectedSceneNodePtr()->rotate(glm::vec3(0.0f, 0.0f, 90.0f));
            break;
        case GLFW_KEY_RIGHT:
            engine.getSelectedSceneNodePtr()->rotate(glm::vec3(0.0f, 0.0f, -90.0f));
            break;
        case GLFW_KEY_A:
            engine.getSelectedSceneNodePtr()->translate(glm::vec3(-1.0f, 0.0f, 0.0f));
            break;
        case GLFW_KEY_D:
            engine.getSelectedSceneNodePtr()->translate(glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case GLFW_KEY_P:
            break;
    }
}

void tetrisFrame(std::string texture_path)
{
    //create a root object at 0/0/0
    GtsSceneNodeOpt obj1;
    obj1.identifier = "tetrisframeroot";
    engine.addNodeToScene(obj1);

    float lowerboundY = 0.0f;
    float upperboundY = 15.0f;

    float lowerboundX = 0.0f;
    float upperboundX = 11.0f;

    //build up the frame and append all the objects to the root scene node
    for(float x = lowerboundX; x <= upperboundX; x++)
    {
        for(float y = lowerboundY; y <= upperboundY; y++)
        {
            if(y == lowerboundY || x == lowerboundX || x == upperboundX)
            {
                GtsSceneNodeOpt object;
                object.objectPtr = engine.createObject(MODEL_PATH, texture_path);
                object.translationVector = glm::vec3(x - round((upperboundX/2)), -1.0f + y, 0.0f);
                object.parentIdentifier = "tetrisframeroot";
                engine.addNodeToScene(object);
            }
        }
    }
}

void I_tetrisPiece(std::string texture_path, std::string identifier)
{
     //for pieces we need to move the root towards the upper end of the frame
    GtsSceneNodeOpt rootnode;
    rootnode.identifier = identifier;
    rootnode.translationVector = glm::vec3(0.0f, 12.0f, 0.0f);
    //rootnode.rotationVector = left_rotation;
    //rootnode.scaleVector = glm::vec3(1.5f, 1.5f, 1.5f);
    //rootnode.scaleVector = glm::vec3(0.0f, 1.1f, 0.0f);
    rootnode.animPtr = new SlowFallAnimation();
    engine.addNodeToScene(rootnode);

    GtsSceneNodeOpt object;
    object.objectPtr = engine.createObject(MODEL_PATH, texture_path);
    object.translationVector = glm::vec3(0.0f, 0.0f, 0.0f);
    object.parentIdentifier = identifier;
    engine.addNodeToScene(object);

    GtsSceneNodeOpt object1;
    object1.objectPtr = engine.createObject(MODEL_PATH, texture_path);
    object1.translationVector = glm::vec3(0.0f, 1.0f, 0.0f);
    object1.parentIdentifier = identifier;
    engine.addNodeToScene(object1);

    GtsSceneNodeOpt object2;
    object2.objectPtr = engine.createObject(MODEL_PATH, texture_path);
    object2.translationVector = glm::vec3(0.0f, 2.0f, 0.0f);
    object2.parentIdentifier = identifier;
    engine.addNodeToScene(object2);

    GtsSceneNodeOpt object3;
    object3.objectPtr = engine.createObject(MODEL_PATH, texture_path);
    object3.translationVector = glm::vec3(0.0f, -1.0f, 0.0f);
    object3.parentIdentifier = identifier;
    engine.addNodeToScene(object3);
}

// Function to update the Tetris grid with the current position of the tetromino
void updateTetrisGrid(const std::vector<glm::ivec3>& tetrominoGridCoords)
{
    for (const auto& coord : tetrominoGridCoords)
    {
        //x needs an offset of +4
        int modifiedX = coord.x + 4;
        if (coord.y >= 0 && coord.y < 20 && modifiedX >= 0 && modifiedX < 10)
        {
            tetrisGrid[coord.y][modifiedX] = 1;
        }
    }
}

// Function to check for collisions with the bottom of the grid
bool checkCollisionWithBottom(const std::vector<glm::ivec3>& tetrominoGridCoords)
{
    for (const auto& coord : tetrominoGridCoords)
    {
        if (coord.y <= 0)
        {
            return true;
        }
    }
    return false;
}

void printTetrisGrid()
{
    std::cout << "CURRENT GRID" << std::endl;
    std::cout << "----------------------------" << std::endl;

    for (int y = 0; y < 20; ++y)
    {
        for (int x = 0; x < 10; ++x)
        {
            std::cout << (tetrisGrid[y][x] == 0 ? '.' : '#') << ' ';
        }
        std::cout << '\n';
    }
    std::cout << "----------------------------" << std::endl;
}

void onSceneUpdated()
{
   
}

void printTetrominoData()
{
    int i = 1;
    for (const auto& coord : engine.getSelectedSceneNodePtr()->getGridCoordinates())
    {
        std::cout << "Grid Position of Object " << i << ":" << std::endl;
        std::cout << "X: " << coord.x << " Y: " << coord.y << " Z: " << coord.z << std::endl;

        i++;
    }
}

void onTetrominoTransform()
{
    printTetrominoData();

    // Check for collisions or update game state based on these coordinates
    if (checkCollisionWithBottom(engine.getSelectedSceneNodePtr()->getGridCoordinates()))
    {
        std::cout << "Collision with bottom detected! Stopping tetromino and updating grid.\n";
        //engine.getSelectedSceneNodePtr()->undoLastTransform();
        engine.getSelectedSceneNodePtr()->setActive(false);
        updateTetrisGrid(engine.getSelectedSceneNodePtr()->getGridCoordinates());
        printTetrisGrid();
        printTetrominoData();
        
    }

    
}

int main() 
{
    tetrisGrid = std::vector<std::vector<int>>(20, std::vector<int>(10, 0));
    engine = Gravitas();
    engine.init(600, 800, "Tetris", true);
    engine.createEmptyScene();
    tetrisFrame(FRAME_TEXTURE_PATH);
    I_tetrisPiece(I_PIECE_TEXTURE_PATH, "I");
    engine.selectNode("I");
    engine.getSelectedSceneNodePtr()->subscribeToTransformEvent(onTetrominoTransform);

    printTetrisGrid();
    
    engine.subscribeOnKeyPressedEvent(onKeyPressed);
    //engine.subscribeOnSceneUpdatedEvent(onSceneUpdated);
        
    engine.run();


    return EXIT_SUCCESS;
}