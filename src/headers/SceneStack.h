//
//  SceneStack.h
//  raygame2
//
//  Created by apple on 13/02/2025.
//

#ifndef SceneStack_h
#define SceneStack_h
#include <vector>
#include <memory>




class Screen;


class SceneStack
{
public:
    std::vector<std::unique_ptr<Screen>> stack;
    
    char* name;

    SceneStack();
    ~SceneStack();
    
    template <typename T>
    void addScene(std::unique_ptr<T> scene)
    {
        stack.push_back( std::move(scene));
    }
    
    virtual void SetPopupScene(int idx);
    void ClosePopup();
    
    virtual void Update(float dt);
    virtual void Draw();
    

    
    virtual void Unload();
};
/*
class MainMenu_stack:public SceneStack
{
public:

    MainMenu_stack()
    {
        name = "main menu";
        stack.clear();
        //addScene(new MainScreen);
        
    }

//    void Draw()override
//    {
//        SceneStack::Draw();
//    }

    ~MainMenu_stack()
    {

    }
    
    
    
    
};

class BoatScene_stack:public SceneStack
{
public:
    
    
    BoatScene_stack()
    {
        name = "boat scene";
        stack.clear();
        //addScene(new BoatScreen);    }
        
    }
    
    void SetPopupScene(int idx)
    {
        
    }

    ~BoatScene_stack()
    {
        
    }
    
    

    
};
*/


#endif /* SceneStack_h */
