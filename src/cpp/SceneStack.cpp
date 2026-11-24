//
//  SceneStack.cpp
//  raygame2
//
//  Created by apple on 14/02/2025.
//

#include <stdio.h>
#include "SceneStack.h"
#include "Screen.h"
#include "raylib.h"


SceneStack::SceneStack()
{
    
}
SceneStack::~SceneStack()
{
    std::cout << "destoryed" << std::endl;
    for (auto& scene:stack)
    {
        /* code */
        scene->Unload();
    }
    
    stack.clear();

}

void SceneStack::SetPopupScene(int idx)
{
    
}

void SceneStack::PushPopup(std::unique_ptr<Screen> scene)
{
    if (stack.empty())
        throw std::runtime_error("A popup scene requires an active main scene.");
    if (!scene)
        throw std::invalid_argument("Cannot push an empty popup scene.");
    stack.push_back(std::move(scene));
}


void SceneStack::Update(float dt)
{
    if (!stack.empty())
        stack.back()->Update(dt);

    for(auto &s:stack)
    {
        s->DrawTexture();
    }
    

}
void SceneStack::Draw()
{
    for (auto& scene : stack)
        scene->Draw();
}
//template <typename T>
//void SceneStack::addScene(std::unique_ptr<T> scene)
//{
//    stack.push_back( std::move(scene));
//    
//}


bool SceneStack::ClosePopup()
{
    if (stack.size() <= 1)
        return false;
    stack.back()->Unload();
    stack.pop_back();
    return true;
}
void SceneStack::Unload()
{
    for(auto& s: stack)
    {
        s->Unload();
    }
    
    
}

