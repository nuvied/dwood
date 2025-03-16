//
// Created by Naveed on 3/9/2025.
//


#ifndef DEVILWOOD_LENSMANAGER_H
#define DEVILWOOD_LENSMANAGER_H

#include "raylib.h"

class LensManager
{
public:

    Vector2 lensPos;


    void Init();
    void Update();
    void Draw();
    void ResetLens()
    {
        lensPos = {512,256};
    }
private:
    int lensePosLoc;
};

#endif //DEVILWOOD_LENSMANAGER_H
