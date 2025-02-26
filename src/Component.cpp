
#include "Component.h"


TransformComp::TransformComp()
{
    position = {0};
}

TransformComp::TransformComp(float x, float y, float rot)
{
    position.x = x;
    position.y = y;
}


void TransformComp::OnChangePerent()
{
    std::cout << "parent changed" <<std::endl;
    parent = entity->parent->getComponent<TransformComp>();
    //auto offsetpos = position - pt->position;

}
    
Matrix2D TransformComp::getWorldMatrix()const{
    Matrix2D local = Matrix2D::Transform(position, rotation, scale);
    if(parent) return parent->getWorldMatrix()*local;
    return local;
}

Vector2 TransformComp::getWorldPosition() const{
    return getWorldMatrix().TransformVector({0,0});
}

float TransformComp::getWorldRotation()const{
    return parent? parent->getWorldRotation() + rotation :rotation;
}



void TransformComp::Update(float dt)
{
    
}
void TransformComp::Draw() 
{
    
}
    




Sprite::Sprite()
{
    
}
Sprite::Sprite(Texture2D tex, Rectangle rec, Vector2 origin, Color c)
{
    texture = tex;
    src_rec = rec;
    this->origin = origin;
    color = c;
    
}
    // Sprite(Sprite& sp)
    // {
    //     texture = sp.texture;
    //     src_rec = sp.src_rec;
    //     origin = sp.origin;
    //     color = sp.color;
        
    // }
Sprite::~Sprite()
{
    //UnloadTexture(texture);
}
    
void Sprite::setOwner(Entity* e) 
{
    Component::setOwner(e);
    trans = entity->getComponent<TransformComp>();
    
    
}
    
void Sprite::Update(float dt) 
{
    // std::cout << texture.width << std::endl;
    for(auto& c: entity->childs)
    {
        c->Update(dt);
    }
}
    
void Sprite::setOrigin(float x, float y)
{
    origin = {x,y};
}
    
void Sprite::Draw()
{
    //TransformComp* t = entity->getComponent<TransformComp>();
    if(!trans)
        trans = entity->getComponent<TransformComp>(); 
    float a;
    if(entity->parent)
    {
        a = alpha * entity->parent->alpha;
       
        a = Clamp(a, 0,1.0f);
        entity->alpha = a;
    }
    else
    a = alpha;
    if(trans){

        Vector2 worldPos = trans->getWorldPosition();
        float worldRot = trans->getWorldRotation();
        DrawTexturePro(texture,src_rec,
            {worldPos.x,worldPos.y,src_rec.width * trans->scale.x, 
                src_rec.height * trans->scale.y
            }
            ,
            {origin.x * src_rec.width, origin.y * src_rec.height }
            ,
            worldRot, 
            Fade(color, a));
/*
            for(auto& c: entity->childs)
            {
                c->Draw();
            }*/
    }
    else
    {
        DrawTextureRec(texture,src_rec,{0,0}, Fade(color, color.a));
    }
    
    // DrawTexture(ResourcesLoader::ui_page, 0,0,WHITE);
}



ColliderComp::ColliderComp(float x, float y, float w, float h)
{
    col_rect = {x,y,w,h};

    

}

void ColliderComp::setOwner(Entity* e)
{
    Component::setOwner(e);
    sprite = entity->getComponent<Sprite>();
    transform = entity->getComponent<TransformComp>();
    
    SetRect();
}



void ColliderComp::Update(float dt)
{
    /*
    if(CheckCollisionPointRec(Global::mousePos, col_rect))
    {
        if(IsMouseButtonDown(0))
            entity->OnClicked();
    }*/
    SetRect();
}

void ColliderComp::Draw()
{
    if(Global::debug)
    DrawRectangleLines(col_rect.x, col_rect.y, col_rect.width, col_rect.height, RED);
}

void ColliderComp::SetRect()
{
    if(sprite && transform)
    {
        Vector2 worldPos = transform->getWorldPosition();
        col_rect = {worldPos.x, worldPos.y, sprite->src_rec.width, sprite->src_rec.height};
    }
}




CircleCol::CircleCol()
{
    
}
CircleCol::CircleCol(float x, float y, float r)
{
    c = {x, y, r};
}

void CircleCol::setOwner(Entity *e)
{
    Component::setOwner(e);
    if(e)
    {
        transform = e->getComponent<TransformComp>();
    }
}
void CircleCol::Update(float dt)
{
    if(transform)
    {
        c.x = transform->position.x;
        c.y = transform->position.y;
    }
}

void CircleCol::Draw()
{
    
    if(Global::debug){
        DrawCircleLines(c.x, c.y, c.radius, RED);
    }
}




Behaviour::Behaviour()
{
    
}


void Behaviour::setOwner(Entity* e){
    Component::setOwner(e);
    col = entity->getComponent<ColliderComp>();
    circle_col = entity->getComponent<CircleCol>();
    Init();

}

Entity* Behaviour::getEntity(int id)
{
    return nullptr;
}

Entity* Behaviour::getEntity(std::string name)
{
    return Game::get_Instance().getEntity(name);
}

void Behaviour::Init()
{
    
}

void Behaviour::SceneLoaded()
{
    
}

void Behaviour::OnMouseDown()
{
    
    
}
void Behaviour::OnMouseUp()
{
    
}
void Behaviour::OnMouseHeld()
{
    
}
void Behaviour::Update(float dt)
{
    if(circle_col){
        if(CheckCollisionPointCircle(Global::mousePos, {circle_col->c.x, circle_col->c.y}, circle_col->c.radius))
        {
            if(IsMouseButtonPressed(0))
            {
                OnMouseDown();
            }
            else if(IsMouseButtonReleased(0))
            {
                OnMouseUp();
            }
        }
    }
    
    
    //rectangle collision detection
    
    
    if(!col)return;
    
    if(CheckCollisionPointRec(Global::mousePos, col->col_rect))
    {
        if(IsMouseButtonPressed(0))
        {
            OnMouseDown();
            
        }
        
    }
}
void Behaviour::Draw()
{
   // DrawRectangleRec(col->col_rect, Fade(RED, 0.3f));
   // DrawRectangle(Global::mousePos.x, Global::mousePos.y, 64, 32, RED);
}