//
//  Compoenent.h
//  raygame2
//
//  Created by apple on 06/02/2025.
//

#ifndef Compoenent_h
#define Compoenent_h

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <iostream>
#include "Game.hpp"
#include "Global.h"
#include "raymath.h"
#include "SceneStack.h"
#include <string>

#include "BaseComponent.h"
#include "Entity.h"

class Entity;
class TransformComp;
class Panel_Sprite;

typedef struct Circle
{
    float x, y;
    float radius;
    
}Circle;




struct Matrix2D {
    float m[3][3]; // 3x3 Transformation Matrix

    // Initialize as identity matrix
    Matrix2D() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // Multiply two matrices
    Matrix2D operator*(const Matrix2D& other) const {
        Matrix2D result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = m[i][0] * other.m[0][j] +
                                 m[i][1] * other.m[1][j] +
                                 m[i][2] * other.m[2][j];
            }
        }
        return result;
    }

    // Create a transformation matrix from position, rotation, and scale
    static Matrix2D Transform(Vector2 position, float rotation, Vector2 scale) {
        Matrix2D result;
        float cosR = cos(rotation * DEG2RAD);
        float sinR = sin(rotation * DEG2RAD);

        result.m[0][0] = cosR * scale.x;
        result.m[0][1] = -sinR * scale.y;
        result.m[0][2] = position.x;

        result.m[1][0] = sinR * scale.x;
        result.m[1][1] = cosR * scale.y;
        result.m[1][2] = position.y;

        result.m[2][0] = 0;
        result.m[2][1] = 0;
        result.m[2][2] = 1;

        return result;
    }

    // Transform a vector by the matrix
    Vector2 TransformVector(Vector2 v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2],
            m[1][0] * v.x + m[1][1] * v.y + m[1][2]
        };
    }
};




class TransformComp : public Component
{
public:
    Vector2 position = {0,0};
    float rotation = 0.0f;
    Vector2 scale = {1.0f, 1.0f};


    TransformComp* parent = nullptr;
    
    TransformComp();
    TransformComp(float x, float y, float rot = 0.0f);
    
    void OnChangePerent()override;
    Matrix2D getWorldMatrix()const;

    Vector2 getWorldPosition() const;

    float getWorldRotation()const;
    void Update(float dt) override;
    void Draw() override;
    
};


class Sprite :public Component
{
private:
    TransformComp* trans = nullptr;
    Panel_Sprite* panel = nullptr;
public:
    Texture2D texture;
    Rectangle src_rec;
    float alpha = 1.0f;
    Vector2 origin;
    Color color;
    Sprite();
    Sprite(Texture2D tex, Rectangle rec, Vector2 origin = {0}, Color c = WHITE);
    ~Sprite();
    
    void setOwner(Entity* e) override;
    
    void Update(float dt) override;
    
    void setOrigin(float x, float y);
    
    void Draw() override;
};

class Panel_Sprite:public Component
{
public:

    NPatchInfo info;
    Rectangle targetRect;

    Panel_Sprite(float margin, Rectangle rect);
    void Update(float dt) override
    {};
    void Draw() override
    {};
    ~Panel_Sprite()=default;

};


class ColliderComp : public Component
{
private:
    Sprite* sprite = nullptr;
    Panel_Sprite *pSprite = nullptr;
    TransformComp* transform = nullptr;
public:
    
    Rectangle col_rect;
    bool set_to_sprite = false;

    ColliderComp();

    ColliderComp(Rectangle& rec);
    
    ColliderComp(float x, float y, float w, float h);
    
    void setOwner(Entity* e) override;
    
    ~ColliderComp() = default;
    
    void Update(float dt) override;
    
    void Draw() override;
    
private:
    void SetRect();
};

class CircleCol:public Component
{
public:
    TransformComp *transform;
    Circle c;
    
    
    CircleCol();
    CircleCol(float x, float y, float r);
    
    void setOwner(Entity *e) override;
    void Update(float dt) override;
    
    void Draw()override;
};

class Behaviour:public Component
{
public:
    ColliderComp* col = nullptr;
    CircleCol* circle_col = nullptr;
    Behaviour();
    ~Behaviour() = default;
    
    void setOwner(Entity* e) override;

    Entity* getEntity(int id);

    Entity* getEntity(std::string name);

    virtual void Init();
    virtual void Start(){}
    virtual void SceneLoaded();

    virtual void OnMouseDown();
    virtual void OnMouseUp();
    virtual void OnMouseHeld();
    void Update(float dt)override;

    virtual void Draw() override;
    

};


#endif /* Compoenent_h */


