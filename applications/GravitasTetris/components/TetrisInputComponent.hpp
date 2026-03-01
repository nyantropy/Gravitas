#pragma once

// the struct which determines what action happens whenever a user presses a button
struct TetrisInputComponent
{
    bool moveLeft  = false;
    bool moveRight = false;
    bool rotateCW  = false;   // E
    bool rotateCCW = false;   // Q
    bool hardDrop  = false;   // Space
    bool softDrop  = false;   // S

    void clear()
    {
        moveLeft = moveRight = rotateCW = rotateCCW = hardDrop = softDrop = false;
    }
};
