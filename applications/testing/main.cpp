#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasInclude.h"
#include "GtsSceneNodeOpt.h"
#include "GtsSceneNode.hpp"
#include "SlowFallAnimation.hpp"
#include "GtsFrameGrabber.hpp"
#include "RandomNumberGenerator.h"

const std::string MODEL_PATH = "resources/models/cube.obj";
const std::string FRAME_TEXTURE_PATH = "resources/textures/grey_texture.png";
const std::string I_PIECE_TEXTURE_PATH = "resources/textures/cyan_texture.png";
const std::string O_PIECE_TEXTURE_PATH = "resources/textures/yellow_texture.png";
const std::string S_PIECE_TEXTURE_PATH = "resources/textures/red_texture.png";
const std::string Z_PIECE_TEXTURE_PATH = "resources/textures/green_texture.png";
const std::string L_PIECE_TEXTURE_PATH = "resources/textures/orange_texture.png";
const std::string T_PIECE_TEXTURE_PATH = "resources/textures/purple_texture.png";
const std::string J_PIECE_TEXTURE_PATH = "resources/textures/blue_texture.png";

std::vector<std::vector<int>> tetrisGrid;
std::vector<std::vector<GtsSceneNode*>> tetrisGridSceneNodes;
Gravitas engine;
RandomNumberGenerator rng;

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

std::string tetrisPiece(std::string texture_path, std::vector<glm::vec3> coords, glm::vec3 origin)
{
    nodeId++;
    std::string identifier = std::to_string(nodeId);

    //root node
    GtsSceneNodeOpt rootnode;
    rootnode.identifier = identifier;
    rootnode.translationVector = origin;
    rootnode.animPtr = new SlowFallAnimation();
    engine.addNodeToScene(rootnode);

    //a tetromino consists of 4 cubes
    for(glm::vec3 pos : coords)
    {
        nodeId++;
        GtsSceneNodeOpt cube;
        cube.objectPtr = engine.createObject(MODEL_PATH, texture_path);
        cube.translationVector = pos;
        cube.identifier = std::to_string(nodeId);
        cube.parentIdentifier = identifier;
        engine.addNodeToScene(cube);
    }

    return identifier;
}

void printTetrisGrid();

void checkAndClearRowsReworked()
{
    std::vector<int> rowsToClear; // Store the row indices that need to be cleared

    // Check each row for filled cells
    for (int row = 0; row < 20; ++row)
    {
        bool isRowFilled = true;
        for (int col = 0; col < 10; ++col)
        {
            if (tetrisGrid[row][col] == 0)
            {
                isRowFilled = false;
                break;
            }
        }
        if (isRowFilled)
        {
            rowsToClear.push_back(row);
        }
    }

    std::cout << "Row indices to clear: ";

    for(int& i : rowsToClear)
    {
        std::cout << i << " ";
    }

    std::cout << std::endl;

    for(auto it = rowsToClear.rbegin(); it != rowsToClear.rend(); it++)
    {
        int rowToClear = *it;
        std::cout << "clearing row " << rowToClear << "!" << std::endl;

        //clear the row first
        for (int col = 0; col < 10; ++col)
        {
            tetrisGrid[rowToClear][col] = 0;
            GtsSceneNode* node = tetrisGridSceneNodes[rowToClear][col];
            node->disableRendering();
            engine.removeNodeFromScene(node);
            tetrisGridSceneNodes[rowToClear][col] = nullptr;
        }

        std::cout << "Moving rows!" << std::endl;
        // Move the entries down
        for (int row = rowToClear; row < 20 - 1; ++row)
        {
            for (int col = 0; col < 10; ++col)
            {
                std::swap(tetrisGrid[row][col], tetrisGrid[row + 1][col]);
                std::swap(tetrisGridSceneNodes[row][col], tetrisGridSceneNodes[row + 1][col]);

                if(tetrisGridSceneNodes[row][col] != nullptr)
                {
                    tetrisGridSceneNodes[row][col]->translate(glm::vec3(0.0f, -1.0f, 0.0f));
                }               
            }
        }
    }
}

// Function to update the Tetris grid with the current position of the tetromino
void updateTetrisGrid(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();
        std::cout << "Adding cube:    " << glm::to_string(coords) << std::endl;

        //x needs an offset of +5
        int modifiedX = static_cast<int>(std::round(coords.x)) + 5;
        int modifiedY = static_cast<int>(std::round(coords.y));
        std::cout << "X: " << modifiedX << " Y: " << modifiedY << std::endl;

        tetrisGrid[modifiedY][modifiedX] = 1;
        tetrisGridSceneNodes[modifiedY][modifiedX] = cube;
    }  
}

// Function to check for collisions with the bottom of the grid
bool checkCollisionWithBottom(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();

        int modifiedX = static_cast<int>(std::round(coords.x));
        int modifiedY = static_cast<int>(std::round(coords.y));
        
        if (modifiedY < 0)
        {
            return true;
        }
    }

    return false;
}

bool checkCollisionWithWalls(GtsSceneNode* tetromino)
{
    for(auto cube : tetromino->getChildren())
    {
        glm::vec3 coords = cube->getWorldPosition();

        int modifiedX = static_cast<int>(std::round(coords.x));
        int modifiedY = static_cast<int>(std::round(coords.y));

        if (modifiedX < -5 | modifiedX > 4)
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

        int modifiedX = static_cast<int>(std::round(coords.x)) + 5;
        int modifiedY = static_cast<int>(std::round(coords.y));
        
        if (modifiedY >= 0 && modifiedY < 20 && modifiedX >= 0 && modifiedX < 10)
        {
            if (tetrisGrid[modifiedY][modifiedX] == 1)
            {
                return true;
            }
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

void printTetrisGridSceneNodes()
{
    std::cout << "CURRENT GRID SCENE NODES" << std::endl;
    std::cout << "----------------------------" << std::endl;

    for (int y = 0; y < 20; ++y)
    {
        for (int x = 0; x < 10; ++x)
        {
            std::cout << (tetrisGridSceneNodes[y][x] == nullptr ? '.' : '#') << ' ';
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
    checkAndClearRowsReworked();
    printTetrisGrid();
    printTetrisGridSceneNodes();
    nextTetromino();
}

void onTetrominoTransform()
{
    GtsSceneNode* tetromino = engine.getSelectedSceneNodePtr();

    //bottom collision will always spawn a new tetromino
    if(checkCollisionWithBottom(tetromino))
    {
        //std::cout << "Collision with bottom detected! Stopping tetromino and updating grid.\n";
        postCollision(tetromino);
        return; 
    }

    //on wall collision, we undo the last transform
    if(checkCollisionWithWalls(tetromino))
    {
        //std::cout << "Collision with walls detected!\n";
        tetromino->undoLastTransform();
        return;
    }

    //colliding with the grid will also undo the last transformation
    if(checkCollisionWithGrid(tetromino))
    {
        //std::cout << "Collision with grid detected!\n";

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
    glm::vec3 origin = glm::vec3(0.0f, 12.0f, 0.0f);

    switch(rng.next())
    {
        case 0:
            engine.selectNode(tetrisPiece(O_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)
            }
            , origin));
            break;
        case 1:
            engine.selectNode(tetrisPiece(S_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(-1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f)
            }
            , origin));
            break;
        case 2:
            engine.selectNode(tetrisPiece(Z_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(-1.0f, 1.0f, 0.0f)
            }
            , origin));
            break;
        case 3:
            engine.selectNode(tetrisPiece(L_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f)
            }
            , origin));
            break;
        case 4:
            engine.selectNode(tetrisPiece(J_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(1.0f, -1.0f, 0.0f)
            }
            , origin));
            break;
        case 5:
            engine.selectNode(tetrisPiece(T_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(-1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f)
            }
            , origin));
            break;
        case 6:
            engine.selectNode(tetrisPiece(I_PIECE_TEXTURE_PATH,
            {
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(0.0f, 2.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f)
            }
            , origin));
            break;
    }

    engine.getSelectedSceneNodePtr()->subscribeToTransformEvent(onTetrominoTransform);
}

int main() 
{
    rng = RandomNumberGenerator(0,6);
    tetrisGrid = std::vector<std::vector<int>>(20, std::vector<int>(10, 0));
    tetrisGridSceneNodes = std::vector<std::vector<GtsSceneNode*>>(20, std::vector<GtsSceneNode*>(10, 0));
    engine = Gravitas();
    engine.init(600, 800, "Tetris", true);
    engine.createEmptyScene();
    tetrisFrame(FRAME_TEXTURE_PATH);
    nextTetromino();
    engine.subscribeOnKeyPressedEvent(onKeyPressed);
    engine.startEncoder();
    engine.run();

    return EXIT_SUCCESS;
}