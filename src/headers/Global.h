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

#define itm_oar_ready -4


enum RECIPE_TAG
{
    EMPTY,
    OAR,
    WEB,
    NONE
};

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
    int id = 0;
    Rectangle rect = {0};
    RECIPE_TAG recipe_tag = EMPTY;

};

struct Light
{
    float intensity = 1.0f;
    float radius = 0.2f;
    Vector2 position = {500,100};

    unsigned int positionLoc;
    unsigned int radiusLoc;
    unsigned int intensityLoc;
    Light()
    {

    }
    Light(float intes, float rad, Vector2 pos)
    {
        intensity = intes;
        radius = rad;
        position = pos;
    }
};

struct Recipe
{
    /* data */
    std::string name;
    int id;         // ids should start from -100
    RECIPE_TAG tag;


    int item_1;
    int item_2;
    int item_3;

    int result_item;
};




class ItemDB
{
public:
    
    std::array<Item, 4> items_database{
        "bamboo_stick", -1, {0,78,26,26}, OAR,
        "oar_broken", -2, {0,0,26,26}, OAR,
        "rope", -3, {0,52,26,26}, OAR,
        "oar_ready", -4, {0,26,26,26},NONE

    };

    std::array<Recipe, 1> recipes_database
    {
        {"Oar_recipe",-100 ,RECIPE_TAG::OAR,-1,-2,-3,-4} // bamboo stick + broken oar + rope = oar ready
    };

    Item getItem(int id)
    {
        for (int i = 0; i < items_database.size(); i++)
        {
            /* code */
            if(items_database[i].id == id)
                return items_database[i];
        }
        return Item();
    }

    Recipe getRecipe(RECIPE_TAG recipe_tag)
    {
        for (int i = 0; i < recipes_database.size(); i++)
        {
            /* code */
            if(recipes_database[i].tag == recipe_tag)
                return recipes_database[i];
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



#endif 
