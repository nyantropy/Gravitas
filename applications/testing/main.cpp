#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasInclude.h"
#include "RotationAnimation.hpp"
#include "GtsSceneNodeOpt.h"
#include "GtsSceneNode.hpp"
#include "SlowFallAnimation.hpp"

const glm::vec3 right_rotation = glm::vec3(0.0f, 0.0f, -90.0f);
const glm::vec3 left_rotation = glm::vec3(0.0f, 0.0f, 90.0f);

const std::string MODEL_PATH = "resources/models/cube.obj";
const std::string FRAME_TEXTURE_PATH = "resources/textures/grey_texture.png";
const std::string I_PIECE_TEXTURE_PATH = "resources/textures/blue_texture.png";

std::vector<std::vector<int>> tetrisGrid;
std::vector<std::vector<GtsSceneNode*>> tetrisGridSceneNodes;
Gravitas engine;

int nodeId = 0;
int gridCollisionCounter = 0;

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
            std::cout << "left" << std::endl;
            engine.getSelectedSceneNodePtr()->rotate(glm::vec3(0.0f, 0.0f, 90.0f));
            break;
        case GLFW_KEY_RIGHT:
            std::cout << "right" << std::endl;
            engine.getSelectedSceneNodePtr()->rotate(glm::vec3(0.0f, 0.0f, -90.0f));
            break;
        case GLFW_KEY_A:
            engine.getSelectedSceneNodePtr()->translate(glm::vec3(-1.0f, 0.0f, 0.0f), "keyboard");
            break;
        case GLFW_KEY_D:
            engine.getSelectedSceneNodePtr()->translate(glm::vec3(1.0f, 0.0f, 0.0f), "keyboard");
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
void updateTetrisGrid(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();
        std::cout << "Adding cube:    " << glm::to_string(coords) << std::endl;

        //x needs an offset of +5
        int modifiedX = coords.x + 5;
        std::cout << "X: " << modifiedX << " Y: " << coords.y << std::endl;
        if (coords.y >= 0 && coords.y < 20 && modifiedX >= 0 && modifiedX < 10)
        {
            tetrisGrid[coords.y][modifiedX] = 1;
            tetrisGridSceneNodes[coords.y][modifiedX] = cube;
        }  
    }  
}

// Function to check for collisions with the bottom of the grid
bool checkCollisionWithBottom(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();
        
        // Check for collisions with the bottom of the grid
        if (coords.y < 0)
        {
            return true; // Collision with bottom detected
        }
    }

    return false; // No collision with bottom or existing wall below
}

bool checkCollisionBelowTetromino(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();
        int modifiedX = coords.x + 5;

        if (tetrisGrid[coords.y][modifiedX] == 1)
        {
            return true; // Collision with existing tetromino wall below
        }       
    }

    return false;
}

bool checkCollisionWithWalls(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();

        if (coords.x < -5 | coords.x > 4)
        {
            return true;
        }
    }

    return false;
}

bool checkCollisionWithGrid(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();

        // Adjust x coordinate to match grid indices
        int modifiedX = coords.x + 5;
        
        // Check if cube is within the valid grid bounds
        if (coords.y >= 0 && coords.y < 20 && modifiedX >= 0 && modifiedX < 10)
        {
            // Check if the grid cell is already occupied
            if (tetrisGrid[coords.y][modifiedX] == 1)
            {
                return true; // Collision detected with the Tetris grid
            }
        }
    }

    return false; // No collision detected
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

void nextTetromino();

void postCollision(GtsSceneNode* tetromino)
{
    tetromino->disableAnimation();
    tetromino->undoLastTransform();
    updateTetrisGrid(tetromino);
    printTetrisGrid();
    nextTetromino();
}

void onTetrominoTransform()
{
    GtsSceneNode* tetromino = engine.getSelectedSceneNodePtr();

    bool gridCollision = checkCollisionWithGrid(tetromino);
    bool bottomCollision = checkCollisionWithBottom(tetromino);
    bool wallCollision = checkCollisionWithWalls(tetromino);

    //bottom collision will always spawn a new tetromino
    if(bottomCollision)
    {
        std::cout << "Collision with bottom detected! Stopping tetromino and updating grid.\n";
        postCollision(tetromino);
        return; 
    }

    //on wall collision, we undo the last transform
    if(wallCollision)
    {
        std::cout << "Collision with walls detected!\n";
        tetromino->undoLastTransform();
    }

    //colliding with the grid will also undo the last transformation
    if(gridCollision)
    {
        std::cout << "Collision with grid detected!\n";

        if(gridCollisionCounter >= 1)
        {
            postCollision(tetromino);
            gridCollisionCounter = 0;
        }
        else
        {
            tetromino->undoLastTransform();
            gridCollisionCounter++;
        }
    }
    else
    {
        gridCollisionCounter = 0;
    }
}

void nextTetromino()
{
    nodeId++;
    I_tetrisPiece(I_PIECE_TEXTURE_PATH, std::to_string(nodeId));
    engine.selectNode(std::to_string(nodeId));
    engine.getSelectedSceneNodePtr()->subscribeToTransformEvent(onTetrominoTransform);
}

int main() 
{
    tetrisGrid = std::vector<std::vector<int>>(20, std::vector<int>(10, 0));
    tetrisGridSceneNodes = std::vector<std::vector<GtsSceneNode*>>(20, std::vector<GtsSceneNode*>(10, 0));
    engine = Gravitas();
    engine.init(600, 800, "Tetris", true);
    engine.createEmptyScene();
    tetrisFrame(FRAME_TEXTURE_PATH);
    nextTetromino();
    engine.subscribeOnKeyPressedEvent(onKeyPressed);
    engine.run();

    return EXIT_SUCCESS;
}