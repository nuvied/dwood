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
#include <array>
#include <vector>
#include <algorithm>
#include <iostream>
#include <functional>
#ifdef __ANDROID__
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "DEVILWOOD_DEBUG", __VA_ARGS__);

#endif

inline float TweenLinear(float start, float end, float t) {
        return start + (end - start) * t;
    }

class Global
{
    public:
    static Vector2 mousePos;

    static bool debug;


    static bool rotor_puzzle_done;
    static bool lensOn;
    static Vector2 lensPosition;

    static int selectedItemId;
};

struct Item
{
    std::string name = "NULL";
    int id = 1;
    Rectangle rect = {0};

};

struct Light
{
    float intensity;
    float innerRadius;
    float outerRadius;
};

class ItemDB
{
public:
    
    std::array<Item, 4> items_database{
        "bamboo_stick", -1, {0,78,26,26},
        "oar_broken", -2, {0,0,26,26},
        "rope", -3, {0,52,26,26},
        "oar_ready", -4, {0,26,26,26}

    };

    Item getItem(int id)
    {
        for (int i = 0; i < items_database.size(); i++)
        {
            /* code */
            if(items_database[i].id == id)
                return items_database[i];
        }
        return {0};
    }
};

class ItemTracker
{
private:
    std::vector<int> picked_items;

public:
    

    bool hasItem(int id)
    {
        for(int i = 0; i < picked_items.size(); i++)
        {
            if(id == picked_items[i])
                return true;
        }
        return false;
    }
    void pickItem(int id)
    {
        picked_items.push_back(id);
    }
};


#endif /* Global_h */
