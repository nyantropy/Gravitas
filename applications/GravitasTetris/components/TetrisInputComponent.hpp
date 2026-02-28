#pragma once

// the struct which determines what action happens whenever a user presses a button - further functionality can be added here
struct TetrisInputComponent
{
    bool moveLeft  = false;
    bool moveRight = false;
    bool rotate    = false;
    bool softDrop  = false;

    void clear()
    {
        moveLeft = moveRight = rotate = softDrop = false;
    }
};

