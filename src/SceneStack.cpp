//
//  SceneStack.cpp
//  raygame2
//
//  Created by apple on 14/02/2025.
//

#include <stdio.h>
#include "SceneStack.h"
#include "Screen.h"


SceneStack::SceneStack()
{
    
}
SceneStack::~SceneStack()
{
    std::cout << "destoryed" << std::endl;
    stack.clear();
}

void SceneStack::SetPopupScene(int idx)
{
    
}


void SceneStack::Update(float dt)
{
   /* if(stack.size() == 0)return;
    
    if(stack.size() > 1)
    {
        stack.back()->Update(dt);
    }
    else
    {
        auto base = stack.front();
        base->Update(dt);
    }*/
    if(stack.empty() == false)
        stack.back()->Update(dt);
    

}
void SceneStack::Draw()
{
    /*
    if(stack.size() == 0)return;
    if(stack.size() > 1)
    {
        stack.back()->Draw();
    }
    else
    {
        auto base = stack.front();
        base->Draw();
    }
     */
    
    for(auto& s: stack)
    {
        DrawRectangle(0, 0, 1024, 512, Fade(BLACK, 0.55f));
        s->Draw();
    }

   // DrawText(TextFormat("%s stack %i", name, stack.size()), 500, 10, 20, GREEN);
   
}
//template <typename T>
//void SceneStack::addScene(std::unique_ptr<T> scene)
//{
//    stack.push_back( std::move(scene));
//    
//}


void SceneStack::ClosePopup()
{
    stack.pop_back();
}
void SceneStack::Unload()
{
    for(auto& s: stack)
    {
        s->Unload();
    }
    
    
}

