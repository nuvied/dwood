//
//  Global.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef Global_h
#define Global_h

#include "raylib.h"
#include <string>
class Global
{
    public:
    static Vector2 mousePos;
    static bool debug;


    static bool rotor_puzzle_done;
};

struct Item
{
    /* data */
public:
    std::string name;
    int id;
    Rectangle rect;

};

class IconDB
{
public:
    Item oar, bamboo, rope, ready_oar;

    IconDB()
    {
        // broken aor
        oar.name = "broken_oar";
        oar.id = -1;
        oar.rect = {0,0,26,26};

        // bamboo 
        bamboo.name = "bamboo stick";
        bamboo.id = -2;
        bamboo.rect = {0,78,26,26};

        //rope
        rope.name = "rope";
        rope.id = -3;
        rope.rect = {0,52,26,26};

        //ready oar
        ready_oar.name = "ready_oar";
        ready_oar.id = -4;
        ready_oar.rect = {0,26,26,26};
    }

};




#endif /* Global_h */
