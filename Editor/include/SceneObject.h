#pragma once

#include "Component.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

struct EditorObject
{
    std::unique_ptr<Entity> entity;
    std::string atlas;
};

struct EditorSceneGraph
{
    std::vector<std::unique_ptr<Entity>> roots;
    std::vector<Entity*> objects;
};

struct SpriteAtlasFrame
{
    std::string name;
    Rectangle source{};
};

const Texture2D* AtlasTexture(const std::string& name);
std::vector<SpriteAtlasFrame> SpriteAtlasFrames(const std::string& atlas,
                                               std::string* error = nullptr);
EditorObject LoadObject(const nlohmann::json& object);
EditorSceneGraph LoadSceneGraph(const std::vector<nlohmann::json>& objects,
                                bool rejectInvalidObjects = false);
bool SourceFits(const Sprite& sprite);
bool WorldToLocal(const TransformComp* parent, Vector2 world, Vector2& local);
void ReparentPreservingWorld(class SceneDocument& scene, std::size_t childIndex,
                             std::optional<std::size_t> parentIndex);
