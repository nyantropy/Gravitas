#include "TetrominoShape.hpp"

// a static definiton of each and every tetromino shape, which we need because the game is grid based
const TetrominoShape TetrominoShapes[7][4] =
{
    // I
    {
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0},  glm::ivec2{1,0},  glm::ivec2{2,0} }},
        {{ glm::ivec2{1,-1}, glm::ivec2{1,0},  glm::ivec2{1,1},  glm::ivec2{1,2} }},
        {{ glm::ivec2{-1,1}, glm::ivec2{0,1},  glm::ivec2{1,1},  glm::ivec2{2,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0},  glm::ivec2{0,1},  glm::ivec2{0,2} }}
    },
    // O
    {
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }}
    },
    // T
    {
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{1,0} }},
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,-1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{-1,0} }}
    },
    // S
    {
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{-1,1}, glm::ivec2{0,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{1,0},  glm::ivec2{1,1} }},
        {{ glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{-1,1}, glm::ivec2{0,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{1,0},  glm::ivec2{1,1} }}
    },
    // Z
    {
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{1,-1}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1} }},
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{1,-1}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{0,1} }}
    },
    // J
    {
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{1,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{1,-1} }},
        {{ glm::ivec2{-1,-1}, glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0} }},
        {{ glm::ivec2{-1,1},  glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1} }}
    },
    // L
    {
        {{ glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0}, glm::ivec2{-1,1} }},
        {{ glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1}, glm::ivec2{1,1} }},
        {{ glm::ivec2{1,-1}, glm::ivec2{-1,0}, glm::ivec2{0,0}, glm::ivec2{1,0} }},
        {{ glm::ivec2{-1,-1}, glm::ivec2{0,-1}, glm::ivec2{0,0}, glm::ivec2{0,1} }}
    }
};
