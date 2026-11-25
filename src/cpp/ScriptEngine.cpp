#include "ScriptEngine.h"

#include "Component.h"
#include "Entity.h"
#include "Game.hpp"
#include "ProjectConfig.h"
#include "ProjectUI.h"
#include "SceneStack.h"
#include "Screen.h"
#include "UI_Manager.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace
{
Entity* FindEntityRecursive(Entity& entity, const std::string& name)
{
    if (entity.name == name)
        return &entity;
    for (auto& child : entity.childs)
        if (auto* found = FindEntityRecursive(*child, name))
            return found;
    return nullptr;
}

Entity* FindSceneEntity(const std::string& name)
{
    auto& game = Game::get_Instance();
    if (!game.scene_stak || game.scene_stak->stack.empty())
        return nullptr;
    auto* screen = game.scene_stak->stack.back().get();
    if (!screen)
        return nullptr;
    for (auto& root : screen->entities)
        if (auto* found = FindEntityRecursive(*root, name))
            return found;
    return nullptr;
}

std::string ResolveVariableId(const VariableStore& store, VariableScope scope,
                              const std::string& nameOrId)
{
    if (store.Has(scope, nameOrId))
        return nameOrId;
    for (const auto& def : store.Definitions(scope))
        if (def.name == nameOrId)
            return def.id;
    return nameOrId;
}

sol::object VariableToLua(sol::state_view lua, const VariableValue& value)
{
    if (const auto* b = std::get_if<bool>(&value))
        return sol::make_object(lua, *b);
    if (const auto* i = std::get_if<int>(&value))
        return sol::make_object(lua, *i);
    return sol::make_object(lua, std::get<std::string>(value));
}

VariableValue LuaToVariable(const sol::object& value, const VariableValue& existing)
{
    if (std::holds_alternative<bool>(existing)) {
        if (value.is<bool>())
            return value.as<bool>();
        throw std::runtime_error("Expected a boolean value for variable.");
    }
    if (std::holds_alternative<int>(existing)) {
        if (value.is<int>())
            return value.as<int>();
        if (value.is<double>())
            return static_cast<int>(value.as<double>());
        throw std::runtime_error("Expected an integer value for variable.");
    }
    if (value.is<std::string>())
        return value.as<std::string>();
    throw std::runtime_error("Expected a string value for variable.");
}

struct ScriptInstance
{
    std::string asset;
    sol::environment env;
    sol::protected_function onStart;
    sol::protected_function onUpdate;
    sol::protected_function onClick;
};

unsigned char ClampColorChannel(int value)
{
    return static_cast<unsigned char>(std::clamp(value, 0, 255));
}

std::tuple<int, int, int, int> ColorToTuple(const Color& color)
{
    return {color.r, color.g, color.b, color.a};
}

Color TupleToColor(int r, int g, int b, sol::optional<int> a, unsigned char existingAlpha)
{
    return {ClampColorChannel(r), ClampColorChannel(g), ClampColorChannel(b),
            a ? ClampColorChannel(*a) : existingAlpha};
}

std::string NormalizeComponentName(std::string name)
{
    std::string result;
    for (const unsigned char c : name)
        if (std::isalnum(c))
            result.push_back(static_cast<char>(std::tolower(c)));
    for (const std::string suffix : {"component", "comp"}) {
        if (result.size() > suffix.size() &&
            result.compare(result.size() - suffix.size(), suffix.size(), suffix) == 0) {
            result.erase(result.size() - suffix.size());
            break;
        }
    }
    return result;
}

template <typename T>
sol::object ComponentObject(sol::state_view lua, Entity& entity)
{
    if (auto* component = entity.getComponent<T>())
        return sol::make_object(lua, component);
    return sol::make_object(lua, sol::lua_nil);
}

sol::object GetComponentObject(sol::state_view lua, Entity& entity, const std::string& rawName)
{
    const auto name = NormalizeComponentName(rawName);
    if (name == "transform") return ComponentObject<TransformComp>(lua, entity);
    if (name == "sprite") return ComponentObject<Sprite>(lua, entity);
    if (name == "rectangle" || name == "rect") return ComponentObject<RectangleComp>(lua, entity);
    if (name == "collider" || name == "boxcollider") return ComponentObject<ColliderComp>(lua, entity);
    if (name == "circlecollider" || name == "circlecol") return ComponentObject<CircleCol>(lua, entity);
    if (name == "text") return ComponentObject<TextComp>(lua, entity);
    if (name == "light") return ComponentObject<LightComp>(lua, entity);
    if (name == "audio") return ComponentObject<AudioComp>(lua, entity);
    if (name == "interaction") return ComponentObject<InteractionComp>(lua, entity);
    if (name == "hotspot") return ComponentObject<HotspotComp>(lua, entity);
    if (name == "inventoryitem" || name == "pickup") return ComponentObject<InventoryItemComp>(lua, entity);
    if (name == "script") return ComponentObject<ScriptComp>(lua, entity);
    TraceLog(LOG_WARNING, "[GAME] [Lua] Unknown component type: %s", rawName.c_str());
    return sol::make_object(lua, sol::lua_nil);
}

constexpr const char* RefMarkerKey = "__devilwood_ref";

void RegisterRefFunction(sol::state& state)
{
    state.set_function("ref", [](sol::this_state s, sol::optional<std::string> type) -> sol::object {
        sol::state_view lua(s);
        const auto canonical = ScriptEngine::NormalizeRefType(type.value_or("Entity"));
        if (canonical.empty())
            throw std::runtime_error("ref(): unknown object type '" + type.value_or("") +
                                     "'. Use Entity, Transform, Sprite, Rectangle, Collider, "
                                     "CircleCollider, Text, Light, Audio, Interaction, Hotspot, "
                                     "InventoryItem, or Script.");
        sol::table marker = lua.create_table();
        marker[RefMarkerKey] = canonical;
        return marker;
    });
}

std::optional<std::string> RefMarkerType(const sol::object& value)
{
    if (!value.is<sol::table>())
        return std::nullopt;
    sol::table table = value.as<sol::table>();
    sol::object type = table[RefMarkerKey];
    if (!type.is<std::string>())
        return std::nullopt;
    return type.as<std::string>();
}
}

std::string ScriptEngine::NormalizeRefType(const std::string& type)
{
    const auto name = NormalizeComponentName(type);
    if (name.empty() || name == "entity" || name == "object" || name == "gameobject")
        return "Entity";
    if (name == "transform") return "Transform";
    if (name == "sprite") return "Sprite";
    if (name == "rectangle" || name == "rect") return "Rectangle";
    if (name == "collider" || name == "boxcollider") return "Collider";
    if (name == "circlecollider" || name == "circlecol") return "CircleCollider";
    if (name == "text") return "Text";
    if (name == "light") return "Light";
    if (name == "audio") return "Audio";
    if (name == "interaction") return "Interaction";
    if (name == "hotspot") return "Hotspot";
    if (name == "inventoryitem" || name == "pickup") return "InventoryItem";
    if (name == "script") return "Script";
    return {};
}

std::string ScriptEngine::RefTypeComponentKey(const std::string& canonicalType)
{
    static const std::unordered_map<std::string, std::string> keys = {
        {"Transform", "TransformComp"}, {"Sprite", "Sprite"}, {"Rectangle", "RectangleComp"},
        {"Collider", "ColliderComp"}, {"CircleCollider", "CircleCol"}, {"Text", "TextComp"},
        {"Light", "LightComp"}, {"Audio", "AudioComp"}, {"Interaction", "InteractionComp"},
        {"Hotspot", "HotspotComp"}, {"InventoryItem", "InventoryItemComp"}, {"Script", "ScriptComp"}};
    const auto it = keys.find(canonicalType);
    return it == keys.end() ? std::string{} : it->second;
}


struct ScriptEngine::Impl
{
    std::unique_ptr<sol::state> lua;
    std::unordered_map<int, ScriptInstance> instances;
    int nextInstanceId = 1;

    void EnsureInitialized()
    {
        if (lua)
            return;
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                            sol::lib::table, sol::lib::utf8);
        RegisterAPI(*lua);
    }

    static ScriptInstance* FindInstance(int instanceId)
    {
        auto& engineImpl = Game::get_Instance().scriptEngine.impl;
        if (!engineImpl || instanceId <= 0)
            return nullptr;
        const auto it = engineImpl->instances.find(instanceId);
        return it == engineImpl->instances.end() ? nullptr : &it->second;
    }

    static void RegisterComponentAPI(sol::state& state, sol::usertype<Entity>& entityType)
    {
        entityType["get_component"] = [](Entity& e, const std::string& name, sol::this_state s) {
            return GetComponentObject(sol::state_view(s), e, name);
        };
        entityType["has_component"] = [](Entity& e, const std::string& name, sol::this_state s) {
            const auto component = GetComponentObject(sol::state_view(s), e, name);
            return component.valid() && component.get_type() != sol::type::lua_nil;
        };
        entityType["get_transform"] = [](Entity& e) { return e.getComponent<TransformComp>(); };
        entityType["get_sprite"] = [](Entity& e) { return e.getComponent<Sprite>(); };
        entityType["get_rectangle"] = [](Entity& e) { return e.getComponent<RectangleComp>(); };
        entityType["get_collider"] = [](Entity& e) { return e.getComponent<ColliderComp>(); };
        entityType["get_circle_collider"] = [](Entity& e) { return e.getComponent<CircleCol>(); };
        entityType["get_text_component"] = [](Entity& e) { return e.getComponent<TextComp>(); };
        entityType["get_light"] = [](Entity& e) { return e.getComponent<LightComp>(); };
        entityType["get_audio"] = [](Entity& e) { return e.getComponent<AudioComp>(); };
        entityType["get_interaction"] = [](Entity& e) { return e.getComponent<InteractionComp>(); };
        entityType["get_hotspot"] = [](Entity& e) { return e.getComponent<HotspotComp>(); };
        entityType["get_inventory_item"] = [](Entity& e) { return e.getComponent<InventoryItemComp>(); };
        entityType["get_script"] = [](Entity& e) { return e.getComponent<ScriptComp>(); };

        // Transform
        auto transform = state.new_usertype<TransformComp>("TransformComponent", sol::no_constructor);
        transform["entity"] = sol::readonly_property([](TransformComp& c) { return c.getEntity(); });
        transform["x"] = sol::property([](TransformComp& c) { return c.position.x; },
                                       [](TransformComp& c, float v) { c.position.x = v; });
        transform["y"] = sol::property([](TransformComp& c) { return c.position.y; },
                                       [](TransformComp& c, float v) { c.position.y = v; });
        transform["rotation"] = sol::property([](TransformComp& c) { return c.rotation; },
                                              [](TransformComp& c, float v) { c.rotation = v; });
        transform["scale_x"] = sol::property([](TransformComp& c) { return c.scale.x; },
                                             [](TransformComp& c, float v) { if (v > 0.0f) c.scale.x = v; });
        transform["scale_y"] = sol::property([](TransformComp& c) { return c.scale.y; },
                                             [](TransformComp& c, float v) { if (v > 0.0f) c.scale.y = v; });
        transform["world_x"] = sol::readonly_property([](TransformComp& c) { return c.getWorldPosition().x; });
        transform["world_y"] = sol::readonly_property([](TransformComp& c) { return c.getWorldPosition().y; });
        transform["world_rotation"] = sol::readonly_property([](TransformComp& c) { return c.getWorldRotation(); });
        transform["set_position"] = [](TransformComp& c, float x, float y) { c.position = {x, y}; };
        transform["translate"] = [](TransformComp& c, float dx, float dy) {
            c.position.x += dx;
            c.position.y += dy;
        };
        transform["set_scale"] = [](TransformComp& c, float sx, sol::optional<float> sy) {
            const float y = sy.value_or(sx);
            if (sx > 0.0f && y > 0.0f)
                c.scale = {sx, y};
        };
        transform["rotate"] = [](TransformComp& c, float degrees) { c.rotation += degrees; };

        // Sprite
        auto sprite = state.new_usertype<Sprite>("SpriteComponent", sol::no_constructor);
        sprite["entity"] = sol::readonly_property([](Sprite& c) { return c.getEntity(); });
        sprite["alpha"] = sol::property([](Sprite& c) { return c.alpha; },
                                        [](Sprite& c, float v) { c.alpha = std::clamp(v, 0.0f, 1.0f); });
        sprite["lens_only"] = sol::property([](Sprite& c) { return c.lensOnly; },
                                            [](Sprite& c, bool v) { c.lensOnly = v; });
        sprite["offset_x"] = sol::property([](Sprite& c) { return c.offset.x; },
                                           [](Sprite& c, float v) { c.offset.x = v; });
        sprite["offset_y"] = sol::property([](Sprite& c) { return c.offset.y; },
                                           [](Sprite& c, float v) { c.offset.y = v; });
        sprite["origin_x"] = sol::property([](Sprite& c) { return c.origin.x; },
                                           [](Sprite& c, float v) { c.origin.x = v; });
        sprite["origin_y"] = sol::property([](Sprite& c) { return c.origin.y; },
                                           [](Sprite& c, float v) { c.origin.y = v; });
        sprite["draw_order"] = sol::property([](Sprite& c) { return c.drawOrder; },
                                             [](Sprite& c, int v) {
                                                 if (auto* owner = c.getEntity())
                                                     owner->setDrawOrder(&c, v);
                                                 else
                                                     c.drawOrder = v;
                                             });
        sprite["texture_width"] = sol::readonly_property([](Sprite& c) { return c.texture.width; });
        sprite["texture_height"] = sol::readonly_property([](Sprite& c) { return c.texture.height; });
        sprite["get_color"] = [](Sprite& c) { return ColorToTuple(c.color); };
        sprite["set_color"] = [](Sprite& c, int r, int g, int b, sol::optional<int> a) {
            c.color = TupleToColor(r, g, b, a, c.color.a);
        };
        sprite["get_source"] = [](Sprite& c) {
            return std::make_tuple(c.src_rec.x, c.src_rec.y, c.src_rec.width, c.src_rec.height);
        };
        sprite["set_source"] = [](Sprite& c, float x, float y, float w, float h) {
            c.src_rec = {x, y, w, h};
        };
        sprite["set_origin"] = [](Sprite& c, float x, float y) { c.setOrigin(x, y); };

        // Rectangle
        auto rectangle = state.new_usertype<RectangleComp>("RectangleComponent", sol::no_constructor);
        rectangle["entity"] = sol::readonly_property([](RectangleComp& c) { return c.getEntity(); });
        rectangle["width"] = sol::property([](RectangleComp& c) { return c.width; },
                                           [](RectangleComp& c, float v) { c.width = std::max(0.0f, v); });
        rectangle["height"] = sol::property([](RectangleComp& c) { return c.height; },
                                            [](RectangleComp& c, float v) { c.height = std::max(0.0f, v); });
        rectangle["opacity"] = sol::property([](RectangleComp& c) { return c.opacity; },
                                             [](RectangleComp& c, float v) { c.opacity = std::clamp(v, 0.0f, 1.0f); });
        rectangle["get_color"] = [](RectangleComp& c) { return ColorToTuple(c.color); };
        rectangle["set_color"] = [](RectangleComp& c, int r, int g, int b, sol::optional<int> a) {
            c.color = TupleToColor(r, g, b, a, c.color.a);
        };

        // Box collider
        auto collider = state.new_usertype<ColliderComp>("ColliderComponent", sol::no_constructor);
        collider["entity"] = sol::readonly_property([](ColliderComp& c) { return c.getEntity(); });
        collider["get_rect"] = [](ColliderComp& c) {
            return std::make_tuple(c.col_rect.x, c.col_rect.y, c.col_rect.width, c.col_rect.height);
        };
        collider["contains"] = [](ColliderComp& c, float x, float y) {
            return CheckCollisionPointRec({x, y}, c.col_rect);
        };

        // Circle collider
        auto circle = state.new_usertype<CircleCol>("CircleColliderComponent", sol::no_constructor);
        circle["entity"] = sol::readonly_property([](CircleCol& c) { return c.getEntity(); });
        circle["radius"] = sol::property([](CircleCol& c) { return c.localRadius; },
                                         [](CircleCol& c, float v) { c.localRadius = std::max(0.0f, v); });
        circle["get_circle"] = [](CircleCol& c) { return std::make_tuple(c.c.x, c.c.y, c.c.radius); };
        circle["contains"] = [](CircleCol& c, float x, float y) {
            return CheckCollisionPointCircle({x, y}, {c.c.x, c.c.y}, c.c.radius);
        };

        // Text
        auto text = state.new_usertype<TextComp>("TextComponent", sol::no_constructor);
        text["entity"] = sol::readonly_property([](TextComp& c) { return c.getEntity(); });
        text["text"] = sol::property([](TextComp& c) { return c.Text(); },
                                     [](TextComp& c, const std::string& v) { c.SetText(v); });
        text["font"] = sol::readonly_property([](TextComp& c) { return c.FontName(); });
        text["measure"] = [](TextComp& c) {
            const auto size = c.Measure();
            return std::make_tuple(size.x, size.y);
        };

        // Light
        auto light = state.new_usertype<LightComp>("LightComponent", sol::no_constructor);
        light["entity"] = sol::readonly_property([](LightComp& c) { return c.getEntity(); });
        light["intensity"] = sol::property([](LightComp& c) { return c.intensity; },
                                           [](LightComp& c, float v) { c.intensity = std::max(0.0f, v); });
        light["sprite_intensity"] = sol::property([](LightComp& c) { return c.spriteIntensity; },
                                                  [](LightComp& c, float v) { c.spriteIntensity = std::max(0.0f, v); });
        light["radius"] = sol::property([](LightComp& c) { return c.radius; },
                                        [](LightComp& c, float v) { c.radius = std::max(0.0f, v); });
        light["use_sprite_shape"] = sol::property([](LightComp& c) { return c.useSpriteShape; },
                                                  [](LightComp& c, bool v) { c.useSpriteShape = v; });
        light["also_use_point_light"] = sol::property([](LightComp& c) { return c.alsoUsePointLight; },
                                                      [](LightComp& c, bool v) { c.alsoUsePointLight = v; });
        light["get_color"] = [](LightComp& c) { return ColorToTuple(c.color); };
        light["set_color"] = [](LightComp& c, int r, int g, int b, sol::optional<int> a) {
            c.color = TupleToColor(r, g, b, a, c.color.a);
        };

        // Audio
        auto audio = state.new_usertype<AudioComp>("AudioComponent", sol::no_constructor);
        audio["entity"] = sol::readonly_property([](AudioComp& c) { return c.getEntity(); });
        audio["asset"] = sol::readonly_property([](AudioComp& c) { return c.Asset(); });
        audio["play"] = [](AudioComp& c) { c.Play(); };
        audio["pause"] = [](AudioComp& c) { c.Pause(); };
        audio["stop"] = [](AudioComp& c) { c.Stop(); };

        // Interaction
        auto interaction = state.new_usertype<InteractionComp>("InteractionComponent", sol::no_constructor);
        interaction["entity"] = sol::readonly_property([](InteractionComp& c) { return c.getEntity(); });
        interaction["run_on_scene_start"] = sol::property(
            [](InteractionComp& c) { return c.runOnSceneStart; },
            [](InteractionComp& c, bool v) { c.runOnSceneStart = v; });
        interaction["action_count"] = sol::readonly_property(
            [](InteractionComp& c) { return static_cast<int>(c.actions.size()); });
        interaction["trigger"] = [](InteractionComp& c) { c.Trigger(); };
        interaction["is_running"] = [](InteractionComp& c) { return c.IsRunning(); };

        // Hotspot
        auto hotspot = state.new_usertype<HotspotComp>("HotspotComponent", sol::no_constructor);
        hotspot["entity"] = sol::readonly_property([](HotspotComp& c) { return c.getEntity(); });
        hotspot["enabled"] = sol::property(
            [](HotspotComp& c) { return c.getEntity() && c.getEntity()->isActive(); },
            [](HotspotComp& c, bool v) { if (auto* owner = c.getEntity()) owner->setActive(v); });
        hotspot["check_variable"] = sol::property([](HotspotComp& c) { return c.checkVariable; },
                                                  [](HotspotComp& c, bool v) { c.checkVariable = v; });
        hotspot["variable_scope"] = sol::property(
            [](HotspotComp& c) { return std::string(c.variableScope == VariableScope::Scene ? "scene" : "global"); },
            [](HotspotComp& c, const std::string& v) {
                c.variableScope = (v == "scene" || v == "Scene") ? VariableScope::Scene : VariableScope::Global;
            });
        hotspot["variable"] = sol::property(
            [](HotspotComp& c) { return c.variableId; },
            [](HotspotComp& c, const std::string& v) {
                c.variableId = ResolveVariableId(Game::get_Instance().variables, c.variableScope, v);
            });
        hotspot["require_item"] = sol::property([](HotspotComp& c) { return c.requireItem; },
                                                [](HotspotComp& c, bool v) { c.requireItem = v; });
        hotspot["required_item"] = sol::property([](HotspotComp& c) { return c.requiredItemId; },
                                                 [](HotspotComp& c, int v) { c.requiredItemId = v; });
        hotspot["consume_item"] = sol::property([](HotspotComp& c) { return c.consumeItem; },
                                                [](HotspotComp& c, bool v) { c.consumeItem = v; });
        hotspot["disable_on_success"] = sol::property([](HotspotComp& c) { return c.disableOnSuccess; },
                                                      [](HotspotComp& c, bool v) { c.disableOnSuccess = v; });
        hotspot["fail_message"] = sol::property([](HotspotComp& c) { return c.failMessage; },
                                                [](HotspotComp& c, const std::string& v) { c.failMessage = v; });
        hotspot["trigger"] = [](HotspotComp& c) { c.Trigger(); };
        hotspot["contains"] = [](HotspotComp& c, float x, float y) { return c.Contains({x, y}); };

        // Inventory pickup
        auto pickup = state.new_usertype<InventoryItemComp>("InventoryItemComponent", sol::no_constructor);
        pickup["entity"] = sol::readonly_property([](InventoryItemComp& c) { return c.getEntity(); });
        pickup["item_id"] = sol::property([](InventoryItemComp& c) { return c.itemId; },
                                          [](InventoryItemComp& c, int v) { c.itemId = v; });
        pickup["collect"] = [](InventoryItemComp& c) { return c.Collect(); };
        pickup["is_collecting"] = [](InventoryItemComp& c) { return c.IsCollecting(); };

        // Script (talk to another script instance)
        auto script = state.new_usertype<ScriptComp>("ScriptComponent", sol::no_constructor);
        script["entity"] = sol::readonly_property([](ScriptComp& c) { return c.getEntity(); });
        script["script"] = sol::readonly_property([](ScriptComp& c) { return c.Script(); });
        script["is_loaded"] = [](ScriptComp& c) { return FindInstance(c.InstanceId()) != nullptr; };
        script["get"] = [](ScriptComp& c, const std::string& name, sol::this_state s) -> sol::object {
            auto* instance = FindInstance(c.InstanceId());
            if (!instance)
                return sol::make_object(sol::state_view(s), sol::lua_nil);
            sol::object value = instance->env[name];
            return value;
        };
        script["set"] = [](ScriptComp& c, const std::string& name, sol::object value) {
            auto* instance = FindInstance(c.InstanceId());
            if (!instance) {
                TraceLog(LOG_WARNING, "[GAME] [Lua] Script '%s' is not loaded; cannot set '%s'.",
                         c.Script().c_str(), name.c_str());
                return;
            }
            instance->env[name] = value;
            sol::object props = instance->env["properties"];
            if (props.is<sol::table>()) {
                sol::table table = props.as<sol::table>();
                if (table[name].valid())
                    table[name] = value;
            }
        };
        script["call"] = [](ScriptComp& c, const std::string& functionName,
                            sol::variadic_args args, sol::this_state s) -> sol::object {
            sol::state_view lua(s);
            auto* instance = FindInstance(c.InstanceId());
            if (!instance)
                return sol::make_object(lua, sol::lua_nil);
            sol::object fnObj = instance->env[functionName];
            if (!fnObj.is<sol::protected_function>()) {
                TraceLog(LOG_ERROR, "[GAME] [Lua] Function '%s' not found in %s",
                         functionName.c_str(), instance->asset.c_str());
                return sol::make_object(lua, sol::lua_nil);
            }
            std::vector<sol::object> forwarded(args.begin(), args.end());
            auto fn = fnObj.as<sol::protected_function>();
            sol::protected_function_result result = fn(c.getEntity(), sol::as_args(forwarded));
            if (!result.valid()) {
                sol::error err = result;
                TraceLog(LOG_ERROR, "[GAME] [Lua] call %s:%s failed: %s",
                         instance->asset.c_str(), functionName.c_str(), err.what());
                return sol::make_object(lua, sol::lua_nil);
            }
            if (result.return_count() == 0)
                return sol::make_object(lua, sol::lua_nil);
            sol::object first = result.get<sol::object>();
            return first;
        };
    }

    static void RegisterAPI(sol::state& state)
    {
        state.set_function("log", [](const std::string& message) {
            TraceLog(LOG_INFO, "[GAME] [Lua] %s", message.c_str());
        });
        RegisterRefFunction(state);

        auto entityType = state.new_usertype<Entity>("Entity", sol::no_constructor);
        entityType["name"] = &Entity::name;
        entityType["is_active"] = [](Entity& e) { return e.isActive(); };
        entityType["set_active"] = [](Entity& e, bool active) { e.setActive(active); };
        entityType["get_x"] = [](Entity& e) -> float {
            const auto* t = e.getComponent<TransformComp>();
            return t ? t->position.x : 0.0f;
        };
        entityType["get_y"] = [](Entity& e) -> float {
            const auto* t = e.getComponent<TransformComp>();
            return t ? t->position.y : 0.0f;
        };
        entityType["set_position"] = [](Entity& e, float x, float y) {
            if (auto* t = e.getComponent<TransformComp>())
                t->position = {x, y};
        };
        entityType["get_rotation"] = [](Entity& e) -> float {
            const auto* t = e.getComponent<TransformComp>();
            return t ? t->rotation : 0.0f;
        };
        entityType["set_rotation"] = [](Entity& e, float rotation) {
            if (auto* t = e.getComponent<TransformComp>())
                t->rotation = rotation;
        };
        entityType["get_scale_x"] = [](Entity& e) -> float {
            const auto* t = e.getComponent<TransformComp>();
            return t ? t->scale.x : 1.0f;
        };
        entityType["get_scale_y"] = [](Entity& e) -> float {
            const auto* t = e.getComponent<TransformComp>();
            return t ? t->scale.y : 1.0f;
        };
        entityType["set_scale"] = [](Entity& e, float sx, float sy) {
            if (auto* t = e.getComponent<TransformComp>(); t && sx > 0.0f && sy > 0.0f)
                t->scale = {sx, sy};
        };
        entityType["get_alpha"] = [](Entity& e) -> float {
            if (const auto* s = e.getComponent<Sprite>())
                return s->alpha;
            if (const auto* r = e.getComponent<RectangleComp>())
                return r->opacity;
            return e.alpha;
        };
        entityType["set_alpha"] = [](Entity& e, float alpha) {
            const float clamped = std::clamp(alpha, 0.0f, 1.0f);
            if (auto* s = e.getComponent<Sprite>())
                s->alpha = clamped;
            if (auto* r = e.getComponent<RectangleComp>())
                r->opacity = clamped;
            e.alpha = clamped;
        };
        entityType["get_text"] = [](Entity& e) -> std::string {
            const auto* t = e.getComponent<TextComp>();
            return t ? t->Text() : std::string{};
        };
        entityType["set_text"] = [](Entity& e, const std::string& text) {
            if (auto* t = e.getComponent<TextComp>())
                t->SetText(text);
        };
        entityType["get_child"] = [](Entity& e, const std::string& name) -> Entity* {
            return e.getChild(name);
        };
        entityType["trigger_interaction"] = [](Entity& e) {
            if (auto* interaction = e.getComponent<InteractionComp>())
                interaction->Trigger();
        };
        entityType["get_parent"] = [](Entity& e) -> Entity* { return e.parent; };
        RegisterComponentAPI(state, entityType);

        auto sceneTable = state.create_named_table("scene");
        sceneTable.set_function("find", &FindSceneEntity);
        sceneTable.set_function("change", [](const std::string& sceneFile) {
            auto& game = Game::get_Instance();
            if (game.scene_manager)
                game.ChangeSceneF(game.scene_manager->Directory() / sceneFile);
        });
        sceneTable.set_function("open_popup", [](const std::string& sceneFile, sol::optional<float> duration) {
            auto& game = Game::get_Instance();
            if (game.scene_manager)
                game.OpenPopupSceneF(game.scene_manager->Directory() / sceneFile,
                                     duration.value_or(0.5f));
        });
        sceneTable.set_function("close_popup", [](sol::optional<float> duration) {
            Game::get_Instance().ClosePopupSceneF(duration.value_or(0.5f));
        });

        auto varsTable = state.create_named_table("vars");
        varsTable.set_function("get_global", [](sol::this_state s, const std::string& nameOrId) -> sol::object {
            sol::state_view luaView(s);
            const auto& store = Game::get_Instance().variables;
            const auto id = ResolveVariableId(store, VariableScope::Global, nameOrId);
            if (!store.Has(VariableScope::Global, id))
                return sol::nil;
            return VariableToLua(luaView, store.Get(VariableScope::Global, id));
        });
        varsTable.set_function("set_global", [](const std::string& nameOrId, sol::object value) {
            auto& store = Game::get_Instance().variables;
            const auto id = ResolveVariableId(store, VariableScope::Global, nameOrId);
            if (!store.Has(VariableScope::Global, id)) {
                TraceLog(LOG_ERROR, "[GAME] Script error: global variable not found: %s", nameOrId.c_str());
                return;
            }
            const auto& existing = store.Get(VariableScope::Global, id);
            store.Set(VariableScope::Global, id, LuaToVariable(value, existing));
            TraceLog(LOG_INFO, "[GAME] [Lua] Variable set (Global): %s", nameOrId.c_str());
        });
        varsTable.set_function("get_scene", [](sol::this_state s, const std::string& nameOrId) -> sol::object {
            sol::state_view luaView(s);
            const auto& store = Game::get_Instance().variables;
            const auto id = ResolveVariableId(store, VariableScope::Scene, nameOrId);
            if (!store.Has(VariableScope::Scene, id))
                return sol::nil;
            return VariableToLua(luaView, store.Get(VariableScope::Scene, id));
        });
        varsTable.set_function("set_scene", [](const std::string& nameOrId, sol::object value) {
            auto& store = Game::get_Instance().variables;
            const auto id = ResolveVariableId(store, VariableScope::Scene, nameOrId);
            if (!store.Has(VariableScope::Scene, id)) {
                TraceLog(LOG_ERROR, "[GAME] Script error: scene variable not found: %s", nameOrId.c_str());
                return;
            }
            const auto& existing = store.Get(VariableScope::Scene, id);
            store.Set(VariableScope::Scene, id, LuaToVariable(value, existing));
            TraceLog(LOG_INFO, "[GAME] [Lua] Variable set (Scene): %s", nameOrId.c_str());
        });

        auto invTable = state.create_named_table("inventory");
        invTable.set_function("add", [](int id) { Game::get_Instance().AddItem(id); });
        invTable.set_function("remove", [](int id) { Game::get_Instance().removeItem(id); });
        invTable.set_function("has", [](int id) { return Game::get_Instance().hasItem(id); });
        invTable.set_function("selected", []() { return Global::selectedItemId; });
        invTable.set_function("select", [](int id) { Game::get_Instance().SelectItem(id); });

        auto uiTable = state.create_named_table("ui");
        uiTable.set_function("show", [](const std::string& id) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id)) {
                m->projectUI->SetVisible(id, true);
                m->projectUI->SetEnabled(id, true);
            }
        });
        uiTable.set_function("hide", [](const std::string& id) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id)) {
                m->projectUI->SetVisible(id, false);
                m->projectUI->SetEnabled(id, false);
            }
        });
        uiTable.set_function("is_visible", [](const std::string& id) -> bool {
            auto* m = Game::get_Instance().ui_m;
            return m && m->projectUI && m->projectUI->HasElement(id) && m->projectUI->IsVisible(id);
        });
        uiTable.set_function("enable", [](const std::string& id) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id))
                m->projectUI->SetEnabled(id, true);
        });
        uiTable.set_function("disable", [](const std::string& id) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id))
                m->projectUI->SetEnabled(id, false);
        });
        uiTable.set_function("fade_in", [](const std::string& id, sol::optional<float> dur) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id) && m->projectUI->IsGroup(id))
                m->projectUI->FadeGroup(id, true, dur.value_or(0.5f));
        });
        uiTable.set_function("fade_out", [](const std::string& id, sol::optional<float> dur) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI && m->projectUI->HasElement(id) && m->projectUI->IsGroup(id))
                m->projectUI->FadeGroup(id, false, dur.value_or(0.5f));
        });
        uiTable.set_function("subtitle", [](const std::string& text) {
            auto* m = Game::get_Instance().ui_m;
            if (m && m->projectUI) {
                try {
                    m->projectUI->ShowSubtitle(text);
                } catch (const std::exception& e) {
                    TraceLog(LOG_ERROR, "[GAME] Script subtitle failed: %s", e.what());
                }
            }
        });

        auto soundTable = state.create_named_table("sound");
        soundTable.set_function("on", [](int id) {
            auto& sm = Game::get_Instance().soundManager;
            if (sm.HasSound(id)) sm.On(id);
        });
        soundTable.set_function("off", [](int id) {
            auto& sm = Game::get_Instance().soundManager;
            if (sm.HasSound(id)) sm.Off(id);
        });
        soundTable.set_function("fade_in", [](int id, sol::optional<float> dur) {
            auto& sm = Game::get_Instance().soundManager;
            if (sm.HasSound(id)) sm.FadeIn(id, dur.value_or(1.0f));
        });
        soundTable.set_function("fade_out", [](int id, sol::optional<float> dur) {
            auto& sm = Game::get_Instance().soundManager;
            if (sm.HasSound(id)) sm.FadeOut(id, dur.value_or(1.0f));
        });

        auto inputTable = state.create_named_table("input");
        inputTable.set_function("mouse_x", []() { return Global::mousePos.x; });
        inputTable.set_function("mouse_y", []() { return Global::mousePos.y; });
        inputTable.set_function("mouse_pressed", []() { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); });
        inputTable.set_function("mouse_down", []() { return IsMouseButtonDown(MOUSE_BUTTON_LEFT); });
        inputTable.set_function("right_pressed", []() { return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT); });

        auto saveTable = state.create_named_table("save");
        saveTable.set_function("save", [](sol::optional<int> slot) {
            return Game::get_Instance().SaveGame(slot.value_or(1));
        });
        saveTable.set_function("load", [](sol::optional<int> slot) {
            return Game::get_Instance().LoadGame(slot.value_or(1));
        });
        saveTable.set_function("exists", [](sol::optional<int> slot) {
            return Game::get_Instance().HasSaveGame(slot.value_or(1));
        });
        saveTable.set_function("delete", [](sol::optional<int> slot) {
            return Game::get_Instance().DeleteSaveGame(slot.value_or(1));
        });

        auto lensTable = state.create_named_table("lens");
        lensTable.set_function("enable", []() { Game::get_Instance().SetLensEnabled(true); });
        lensTable.set_function("disable", []() { Game::get_Instance().SetLensEnabled(false); });
        lensTable.set_function("toggle", []() { Game::get_Instance().SetLenseActive(); });
        lensTable.set_function("is_enabled", []() { return Game::get_Instance().IsLensEnabled(); });
    }
};

ScriptEngine::ScriptEngine() : impl(std::make_unique<Impl>()) {}
ScriptEngine::~ScriptEngine() = default;

bool ScriptEngine::IsScriptAssetName(const std::string& name)
{
    if (name.empty() || name.size() > 255 || name.find('\0') != std::string::npos)
        return false;
    const std::filesystem::path path(name);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& part : path)
        if (part == "." || part == "..")
            return false;
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".lua";
}

std::vector<std::string> ScriptEngine::AvailableScripts()
{
    std::vector<std::string> result;
    const auto scriptDirectory = AssetPaths::Script("");
    std::error_code ec;
    if (!std::filesystem::is_directory(scriptDirectory, ec))
        return result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptDirectory, ec)) {
        if (!entry.is_regular_file())
            continue;
        const auto filename = entry.path().filename().string();
        if (!filename.empty() && filename[0] == '.')
            continue;
        const auto relative = entry.path().lexically_relative(scriptDirectory).generic_string();
        if (IsScriptAssetName(relative))
            result.push_back(relative);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ScriptEngine::EnsureApiStubs()
{
    const auto scriptDirectory = AssetPaths::Script("");
    std::error_code ec;
    std::filesystem::create_directories(scriptDirectory, ec);
    if (ec)
        return;

    const auto apiPath = scriptDirectory / ".devilwood_api.lua";
    {
        std::ofstream output(apiPath, std::ios::binary | std::ios::trunc);
        if (output) {
            output << "---@meta\n"
                   << "-- Auto-generated Devilwood Lua API definitions for VS Code (LuaLS)\n\n"
                   << "---@class Entity\n"
                   << "---@field name string\n"
                   << "local Entity = {}\n"
                   << "---@return boolean\nfunction Entity:is_active() end\n"
                   << "---@param active boolean\nfunction Entity:set_active(active) end\n"
                   << "---@return number\nfunction Entity:get_x() end\n"
                   << "---@return number\nfunction Entity:get_y() end\n"
                   << "---@param x number\n---@param y number\nfunction Entity:set_position(x, y) end\n"
                   << "---@return number\nfunction Entity:get_rotation() end\n"
                   << "---@param degrees number\nfunction Entity:set_rotation(degrees) end\n"
                   << "---@return number\nfunction Entity:get_scale_x() end\n"
                   << "---@return number\nfunction Entity:get_scale_y() end\n"
                   << "---@param sx number\n---@param sy number\nfunction Entity:set_scale(sx, sy) end\n"
                   << "---@return number\nfunction Entity:get_alpha() end\n"
                   << "---@param alpha number\nfunction Entity:set_alpha(alpha) end\n"
                   << "---@return string\nfunction Entity:get_text() end\n"
                   << "---@param text string\nfunction Entity:set_text(text) end\n"
                   << "---@param name string\n---@return Entity|nil\nfunction Entity:get_child(name) end\n"
                   << "function Entity:trigger_interaction() end\n"
                   << "---@return Entity|nil\nfunction Entity:get_parent() end\n"
                   << "---Get a component by type name: Transform, Sprite, Rectangle, Collider, CircleCollider,\n"
                   << "---Text, Light, Audio, Interaction, Hotspot, InventoryItem, Script.\n"
                   << "---@param type_name string\n---@return any\nfunction Entity:get_component(type_name) end\n"
                   << "---@param type_name string\n---@return boolean\nfunction Entity:has_component(type_name) end\n"
                   << "---@return TransformComponent|nil\nfunction Entity:get_transform() end\n"
                   << "---@return SpriteComponent|nil\nfunction Entity:get_sprite() end\n"
                   << "---@return RectangleComponent|nil\nfunction Entity:get_rectangle() end\n"
                   << "---@return ColliderComponent|nil\nfunction Entity:get_collider() end\n"
                   << "---@return CircleColliderComponent|nil\nfunction Entity:get_circle_collider() end\n"
                   << "---@return TextComponent|nil\nfunction Entity:get_text_component() end\n"
                   << "---@return LightComponent|nil\nfunction Entity:get_light() end\n"
                   << "---@return AudioComponent|nil\nfunction Entity:get_audio() end\n"
                   << "---@return InteractionComponent|nil\nfunction Entity:get_interaction() end\n"
                   << "---@return HotspotComponent|nil\nfunction Entity:get_hotspot() end\n"
                   << "---@return InventoryItemComponent|nil\nfunction Entity:get_inventory_item() end\n"
                   << "---@return ScriptComponent|nil\nfunction Entity:get_script() end\n\n"
                   << "---@class TransformComponent\n---@field entity Entity\n---@field x number\n---@field y number\n"
                   << "---@field rotation number\n---@field scale_x number\n---@field scale_y number\n"
                   << "---@field world_x number readonly\n---@field world_y number readonly\n---@field world_rotation number readonly\n"
                   << "local TransformComponent = {}\n"
                   << "---@param x number\n---@param y number\nfunction TransformComponent:set_position(x, y) end\n"
                   << "---@param dx number\n---@param dy number\nfunction TransformComponent:translate(dx, dy) end\n"
                   << "---@param sx number\n---@param sy? number\nfunction TransformComponent:set_scale(sx, sy) end\n"
                   << "---@param degrees number\nfunction TransformComponent:rotate(degrees) end\n\n"
                   << "---@class SpriteComponent\n---@field entity Entity\n---@field alpha number\n---@field lens_only boolean\n"
                   << "---@field offset_x number\n---@field offset_y number\n---@field origin_x number\n---@field origin_y number\n"
                   << "---@field draw_order integer\n---@field texture_width integer readonly\n---@field texture_height integer readonly\n"
                   << "local SpriteComponent = {}\n"
                   << "---@return integer r, integer g, integer b, integer a\nfunction SpriteComponent:get_color() end\n"
                   << "---@param r integer\n---@param g integer\n---@param b integer\n---@param a? integer\nfunction SpriteComponent:set_color(r, g, b, a) end\n"
                   << "---@return number x, number y, number w, number h\nfunction SpriteComponent:get_source() end\n"
                   << "---@param x number\n---@param y number\n---@param w number\n---@param h number\nfunction SpriteComponent:set_source(x, y, w, h) end\n"
                   << "---@param x number\n---@param y number\nfunction SpriteComponent:set_origin(x, y) end\n\n"
                   << "---@class RectangleComponent\n---@field entity Entity\n---@field width number\n---@field height number\n---@field opacity number\n"
                   << "local RectangleComponent = {}\n"
                   << "---@return integer r, integer g, integer b, integer a\nfunction RectangleComponent:get_color() end\n"
                   << "---@param r integer\n---@param g integer\n---@param b integer\n---@param a? integer\nfunction RectangleComponent:set_color(r, g, b, a) end\n\n"
                   << "---@class ColliderComponent\n---@field entity Entity\nlocal ColliderComponent = {}\n"
                   << "---@return number x, number y, number w, number h\nfunction ColliderComponent:get_rect() end\n"
                   << "---@param x number\n---@param y number\n---@return boolean\nfunction ColliderComponent:contains(x, y) end\n\n"
                   << "---@class CircleColliderComponent\n---@field entity Entity\n---@field radius number\nlocal CircleColliderComponent = {}\n"
                   << "---@return number x, number y, number radius\nfunction CircleColliderComponent:get_circle() end\n"
                   << "---@param x number\n---@param y number\n---@return boolean\nfunction CircleColliderComponent:contains(x, y) end\n\n"
                   << "---@class TextComponent\n---@field entity Entity\n---@field text string\n---@field font string readonly\nlocal TextComponent = {}\n"
                   << "---@return number width, number height\nfunction TextComponent:measure() end\n\n"
                   << "---@class LightComponent\n---@field entity Entity\n---@field intensity number\n---@field sprite_intensity number\n"
                   << "---@field radius number\n---@field use_sprite_shape boolean\n---@field also_use_point_light boolean\n"
                   << "local LightComponent = {}\n"
                   << "---@return integer r, integer g, integer b, integer a\nfunction LightComponent:get_color() end\n"
                   << "---@param r integer\n---@param g integer\n---@param b integer\n---@param a? integer\nfunction LightComponent:set_color(r, g, b, a) end\n\n"
                   << "---@class AudioComponent\n---@field entity Entity\n---@field asset string readonly\nlocal AudioComponent = {}\n"
                   << "function AudioComponent:play() end\nfunction AudioComponent:pause() end\nfunction AudioComponent:stop() end\n\n"
                   << "---@class InteractionComponent\n---@field entity Entity\n---@field run_on_scene_start boolean\n---@field action_count integer readonly\n"
                   << "local InteractionComponent = {}\n"
                   << "function InteractionComponent:trigger() end\n---@return boolean\nfunction InteractionComponent:is_running() end\n\n"
                   << "---@class HotspotComponent\n---@field entity Entity\n---@field enabled boolean\n---@field check_variable boolean\n"
                   << "---@field variable_scope \"global\"|\"scene\"\n---@field variable string\n---@field require_item boolean\n"
                   << "---@field required_item integer\n---@field consume_item boolean\n---@field disable_on_success boolean\n---@field fail_message string\n"
                   << "local HotspotComponent = {}\n"
                   << "function HotspotComponent:trigger() end\n"
                   << "---@param x number\n---@param y number\n---@return boolean\nfunction HotspotComponent:contains(x, y) end\n\n"
                   << "---@class InventoryItemComponent\n---@field entity Entity\n---@field item_id integer\nlocal InventoryItemComponent = {}\n"
                   << "---@return boolean\nfunction InventoryItemComponent:collect() end\n"
                   << "---@return boolean\nfunction InventoryItemComponent:is_collecting() end\n\n"
                   << "---@class ScriptComponent\n---@field entity Entity\n---@field script string readonly\nlocal ScriptComponent = {}\n"
                   << "---@return boolean\nfunction ScriptComponent:is_loaded() end\n"
                   << "---Read a global/property value from the other script.\n"
                   << "---@param name string\n---@return any\nfunction ScriptComponent:get(name) end\n"
                   << "---Write a value in the other script (also updates its `properties` table).\n"
                   << "---@param name string\n---@param value any\nfunction ScriptComponent:set(name, value) end\n"
                   << "---Call a function in the other script. Its owner entity is passed as the first argument (self).\n"
                   << "---@param function_name string\n---@param ... any\n---@return any\nfunction ScriptComponent:call(function_name, ...) end\n\n"
                   << "---@param message string\nfunction log(message) end\n\n"
                   << "---Expose an object or component reference slot in the Inspector.\n"
                   << "---@overload fun(): Entity|nil\n"
                   << "---@overload fun(type_name: \"Entity\"): Entity|nil\n"
                   << "---@overload fun(type_name: \"Transform\"): TransformComponent|nil\n"
                   << "---@overload fun(type_name: \"Sprite\"): SpriteComponent|nil\n"
                   << "---@overload fun(type_name: \"Rectangle\"): RectangleComponent|nil\n"
                   << "---@overload fun(type_name: \"Collider\"): ColliderComponent|nil\n"
                   << "---@overload fun(type_name: \"CircleCollider\"): CircleColliderComponent|nil\n"
                   << "---@overload fun(type_name: \"Text\"): TextComponent|nil\n"
                   << "---@overload fun(type_name: \"Light\"): LightComponent|nil\n"
                   << "---@overload fun(type_name: \"Audio\"): AudioComponent|nil\n"
                   << "---@overload fun(type_name: \"Interaction\"): InteractionComponent|nil\n"
                   << "---@overload fun(type_name: \"Hotspot\"): HotspotComponent|nil\n"
                   << "---@overload fun(type_name: \"InventoryItem\"): InventoryItemComponent|nil\n"
                   << "---@overload fun(type_name: \"Script\"): ScriptComponent|nil\n"
                   << "---@param type_name? string\n---@return any\nfunction ref(type_name) end\n\n"
                   << "scene = {}\n"
                   << "---@param name string\n---@return Entity|nil\nfunction scene.find(name) end\n"
                   << "---@param scene_file string\nfunction scene.change(scene_file) end\n"
                   << "---@param scene_file string\n---@param duration? number\nfunction scene.open_popup(scene_file, duration) end\n"
                   << "---@param duration? number\nfunction scene.close_popup(duration) end\n\n"
                   << "vars = {}\n"
                   << "---@param name_or_id string\n---@return boolean|integer|string|nil\nfunction vars.get_global(name_or_id) end\n"
                   << "---@param name_or_id string\n---@param value boolean|integer|string\nfunction vars.set_global(name_or_id, value) end\n"
                   << "---@param name_or_id string\n---@return boolean|integer|string|nil\nfunction vars.get_scene(name_or_id) end\n"
                   << "---@param name_or_id string\n---@param value boolean|integer|string\nfunction vars.set_scene(name_or_id, value) end\n\n"
                   << "inventory = {}\n"
                   << "---@param id integer\nfunction inventory.add(id) end\n"
                   << "---@param id integer\nfunction inventory.remove(id) end\n"
                   << "---@param id integer\n---@return boolean\nfunction inventory.has(id) end\n"
                   << "---@return integer\nfunction inventory.selected() end\n"
                   << "---@param id integer\nfunction inventory.select(id) end\n\n"
                   << "ui = {}\n"
                   << "---@param id string\nfunction ui.show(id) end\n"
                   << "---@param id string\nfunction ui.hide(id) end\n"
                   << "---@param id string\n---@return boolean\nfunction ui.is_visible(id) end\n"
                   << "---@param id string\nfunction ui.enable(id) end\n"
                   << "---@param id string\nfunction ui.disable(id) end\n"
                   << "---@param id string\n---@param duration? number\nfunction ui.fade_in(id, duration) end\n"
                   << "---@param id string\n---@param duration? number\nfunction ui.fade_out(id, duration) end\n"
                   << "---@param text string\nfunction ui.subtitle(text) end\n\n"
                   << "sound = {}\n"
                   << "---@param id integer\nfunction sound.on(id) end\n"
                   << "---@param id integer\nfunction sound.off(id) end\n"
                   << "---@param id integer\n---@param duration? number\nfunction sound.fade_in(id, duration) end\n"
                   << "---@param id integer\n---@param duration? number\nfunction sound.fade_out(id, duration) end\n\n"
                   << "input = {}\n"
                   << "---@return number\nfunction input.mouse_x() end\n"
                   << "---@return number\nfunction input.mouse_y() end\n"
                   << "---@return boolean\nfunction input.mouse_pressed() end\n"
                   << "---@return boolean\nfunction input.mouse_down() end\n"
                   << "---@return boolean\nfunction input.right_pressed() end\n\n"
                   << "save = {}\n"
                   << "---@param slot? integer\n---@return boolean\nfunction save.save(slot) end\n"
                   << "---@param slot? integer\n---@return boolean\nfunction save.load(slot) end\n"
                   << "---@param slot? integer\n---@return boolean\nfunction save.exists(slot) end\n"
                   << "---@param slot? integer\n---@return boolean\nfunction save.delete(slot) end\n\n"
                   << "lens = {}\n"
                   << "function lens.enable() end\n"
                   << "function lens.disable() end\n"
                   << "function lens.toggle() end\n"
                   << "---@return boolean\nfunction lens.is_enabled() end\n";
        }
    }

    const auto luarcPath = scriptDirectory / ".luarc.json";
    if (!std::filesystem::exists(luarcPath, ec)) {
        std::ofstream output(luarcPath, std::ios::binary | std::ios::trunc);
        if (output) {
            output << "{\n"
                   << "  \"runtime.version\": \"Lua 5.4\",\n"
                   << "  \"workspace.library\": [\".devilwood_api.lua\"],\n"
                   << "  \"diagnostics.globals\": [\"log\", \"ref\", \"scene\", \"vars\", \"inventory\", \"ui\", \"sound\", \"input\", \"save\", \"lens\"]\n"
                   << "}\n";
        }
    }
}

std::filesystem::path ScriptEngine::CreateTemplateScript(const std::string& scriptId)
{
    if (scriptId.empty() ||
        scriptId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
            std::string::npos)
        throw std::runtime_error("Script name may contain letters, numbers, underscores, and hyphens only.");
    EnsureApiStubs();
    const std::string filename = scriptId + ".lua";
    const auto path = AssetPaths::Script(filename);
    if (std::filesystem::exists(path))
        throw std::runtime_error("A script with that name already exists: " + filename);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot create script file: " + path.string());
    output << "-- Devilwood Lua Script: " << filename << "\n"
           << "-- Available APIs: self, scene, vars, inventory, ui, sound, input, log(msg)\n"
           << "-- Expose Inspector properties & scene references using a top-level `properties` table:\n"
           << "-- properties = { speed = 90.0, active_on_start = true, target = ref(\"Transform\") }\n\n"
           << "---@param self Entity\n"
           << "function on_start(self)\n"
           << "    log(\"Started script on \" .. self.name)\n"
           << "end\n\n"
           << "---@param self Entity\n"
           << "---@param dt number\n"
           << "function on_update(self, dt)\n"
           << "end\n\n"
           << "---@param self Entity\n"
           << "function on_click(self)\n"
           << "    log(\"Clicked \" .. self.name)\n"
           << "end\n";
    if (!output)
        throw std::runtime_error("Cannot finish writing script file: " + path.string());
    return path;
}

std::vector<ScriptPropertyDefinition> ScriptEngine::InspectProperties(
    const std::string& scriptAsset, std::string* error)
{
    if (error)
        error->clear();
    if (!IsScriptAssetName(scriptAsset))
        return {};
    const auto path = AssetPaths::Script(scriptAsset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        if (error)
            *error = "Script file not found: " + scriptAsset;
        return {};
    }
    const auto writeTime = std::filesystem::last_write_time(path, ec);

    struct CacheEntry
    {
        std::filesystem::file_time_type writeTime{};
        std::vector<ScriptPropertyDefinition> properties;
        std::string error;
    };
    static std::unordered_map<std::string, CacheEntry> cache;
    const auto key = path.string();
    const auto cached = cache.find(key);
    if (!ec && cached != cache.end() && cached->second.writeTime == writeTime) {
        if (error)
            *error = cached->second.error;
        return cached->second.properties;
    }

    CacheEntry entry;
    entry.writeTime = writeTime;
    sol::state temp;
    temp.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                        sol::lib::table, sol::lib::utf8);
    temp.set_function("log", [](const std::string&) {});
    RegisterRefFunction(temp);
    const auto result = temp.safe_script_file(path.string(), sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        entry.error = err.what();
    } else {
        sol::object propsObj = temp["properties"];
        if (propsObj.is<sol::table>()) {
            sol::table propsTable = propsObj.as<sol::table>();
            for (const auto& pair : propsTable) {
                if (!pair.first.is<std::string>())
                    continue;
                const auto name = pair.first.as<std::string>();
                if (name.empty())
                    continue;
                const sol::object& val = pair.second;
                if (val.get_type() == sol::type::boolean) {
                    entry.properties.push_back({name, val.as<bool>()});
                } else if (val.get_type() == sol::type::number) {
                    const double num = val.as<double>();
                    if (std::isfinite(num))
                        entry.properties.push_back({name, num});
                } else if (val.get_type() == sol::type::string) {
                    entry.properties.push_back({name, val.as<std::string>()});
                } else if (const auto refType = RefMarkerType(val)) {
                    entry.properties.push_back({name, false, *refType});
                }
            }
            std::sort(entry.properties.begin(), entry.properties.end(),
                      [](const ScriptPropertyDefinition& a, const ScriptPropertyDefinition& b) {
                          return a.name < b.name;
                      });
        }
    }
    if (error)
        *error = entry.error;
    auto props = entry.properties;
    cache[key] = std::move(entry);
    return props;
}

void ScriptEngine::Reset()
{
    impl->instances.clear();
    impl->lua.reset();
    impl->nextInstanceId = 1;
    impl->EnsureInitialized();
}

void ScriptEngine::Shutdown()
{
    impl->instances.clear();
    impl->lua.reset();
}

int ScriptEngine::CreateInstance(const std::string& scriptAsset, Entity* owner,
                                 const ScriptPropertyMap& properties,
                                 const ScriptObjectRefMap& objectRefs)
{
    if (!IsScriptAssetName(scriptAsset)) {
        TraceLog(LOG_ERROR, "[GAME] Invalid script path: %s", scriptAsset.c_str());
        return 0;
    }
    const auto path = AssetPaths::Script(scriptAsset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        TraceLog(LOG_ERROR, "[GAME] Script file not found: %s", path.string().c_str());
        return 0;
    }
    impl->EnsureInitialized();
    sol::environment env(*impl->lua, sol::create, impl->lua->globals());
    if (owner)
        env["self"] = owner;

    const auto result = impl->lua->safe_script_file(path.string(), env, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        TraceLog(LOG_ERROR, "[GAME] Script load error (%s): %s", scriptAsset.c_str(), err.what());
        return 0;
    }

    sol::object propsObj = env["properties"];
    if (propsObj.is<sol::table>()) {
        sol::table propsTable = propsObj.as<sol::table>();
        for (const auto& pair : propsTable) {
            if (pair.first.is<std::string>())
                env[pair.first.as<std::string>()] = pair.second;
        }
        for (const auto& entry : properties) {
            const auto& name = entry.first;
            std::visit([&](const auto& typed) {
                propsTable[name] = typed;
                env[name] = typed;
            }, entry.second);
        }

        // Resolve object reference properties declared with ref("Type").
        std::vector<std::pair<std::string, std::string>> refProperties;
        for (const auto& pair : propsTable) {
            if (!pair.first.is<std::string>())
                continue;
            if (const auto refType = RefMarkerType(pair.second))
                refProperties.emplace_back(pair.first.as<std::string>(), *refType);
        }
        sol::state_view luaView(*impl->lua);
        for (const auto& [refName, refType] : refProperties) {
            sol::object resolved = sol::make_object(luaView, sol::lua_nil);
            const auto found = objectRefs.find(refName);
            Entity* target = found == objectRefs.end() ? nullptr : found->second;
            if (target) {
                resolved = refType == "Entity" ? sol::make_object(luaView, target)
                                               : GetComponentObject(luaView, *target, refType);
                if (resolved.get_type() == sol::type::lua_nil)
                    TraceLog(LOG_WARNING, "[GAME] Script '%s': '%s' has no %s component (property '%s').",
                             scriptAsset.c_str(), target->name.c_str(), refType.c_str(), refName.c_str());
            }
            propsTable[refName] = resolved;
            env[refName] = resolved;
        }
    } else {
        for (const auto& entry : properties) {
            const auto& name = entry.first;
            std::visit([&](const auto& typed) {
                env[name] = typed;
            }, entry.second);
        }
    }

    ScriptInstance instance;
    instance.asset = scriptAsset;
    instance.env = std::move(env);
    sol::object startObj = instance.env["on_start"];
    if (startObj.is<sol::protected_function>())
        instance.onStart = startObj.as<sol::protected_function>();
    sol::object updateObj = instance.env["on_update"];
    if (updateObj.is<sol::protected_function>())
        instance.onUpdate = updateObj.as<sol::protected_function>();
    sol::object clickObj = instance.env["on_click"];
    if (clickObj.is<sol::protected_function>())
        instance.onClick = clickObj.as<sol::protected_function>();

    const int id = impl->nextInstanceId++;
    impl->instances.emplace(id, std::move(instance));
    TraceLog(LOG_INFO, "[GAME] Loaded script '%s' on '%s'",
             scriptAsset.c_str(), owner ? owner->name.c_str() : "object");
    return id;
}

void ScriptEngine::DestroyInstance(int instanceId)
{
    if (instanceId <= 0 || !impl)
        return;
    impl->instances.erase(instanceId);
}

void ScriptEngine::CallStart(int instanceId, Entity* owner)
{
    const auto it = impl->instances.find(instanceId);
    if (it == impl->instances.end() || !it->second.onStart.valid())
        return;
    const auto result = it->second.onStart(owner);
    if (!result.valid()) {
        sol::error err = result;
        TraceLog(LOG_ERROR, "[GAME] Script on_start error (%s): %s",
                 it->second.asset.c_str(), err.what());
    }
}

void ScriptEngine::CallUpdate(int instanceId, Entity* owner, float dt)
{
    const auto it = impl->instances.find(instanceId);
    if (it == impl->instances.end() || !it->second.onUpdate.valid())
        return;
    const auto result = it->second.onUpdate(owner, dt);
    if (!result.valid()) {
        sol::error err = result;
        TraceLog(LOG_ERROR, "[GAME] Script on_update error (%s): %s",
                 it->second.asset.c_str(), err.what());
    }
}

bool ScriptEngine::HasClickHandler(int instanceId) const
{
    const auto it = impl->instances.find(instanceId);
    return it != impl->instances.end() && it->second.onClick.valid();
}

bool ScriptEngine::CallClick(int instanceId, Entity* owner)
{
    const auto it = impl->instances.find(instanceId);
    if (it == impl->instances.end() || !it->second.onClick.valid())
        return false;
    const auto result = it->second.onClick(owner);
    if (!result.valid()) {
        sol::error err = result;
        TraceLog(LOG_ERROR, "[GAME] Script on_click error (%s): %s",
                 it->second.asset.c_str(), err.what());
    }
    return true;
}

bool ScriptEngine::RunScriptAction(const std::string& scriptAsset,
                                   const std::string& functionName, Entity* owner)
{
    if (!IsScriptAssetName(scriptAsset)) {
        TraceLog(LOG_ERROR, "[GAME] Invalid script path: %s", scriptAsset.c_str());
        return false;
    }
    const auto path = AssetPaths::Script(scriptAsset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        TraceLog(LOG_ERROR, "[GAME] Script file not found: %s", path.string().c_str());
        return false;
    }
    impl->EnsureInitialized();
    sol::environment env(*impl->lua, sol::create, impl->lua->globals());
    if (owner)
        env["self"] = owner;
    const auto loadResult = impl->lua->safe_script_file(path.string(), env, sol::script_pass_on_error);
    if (!loadResult.valid()) {
        sol::error err = loadResult;
        TraceLog(LOG_ERROR, "[GAME] RunScript error (%s): %s", scriptAsset.c_str(), err.what());
        return false;
    }
    if (!functionName.empty()) {
        sol::object fnObj = env[functionName];
        if (!fnObj.is<sol::protected_function>()) {
            TraceLog(LOG_ERROR, "[GAME] RunScript function '%s' not found in %s",
                     functionName.c_str(), scriptAsset.c_str());
            return false;
        }
        const auto fnResult = fnObj.as<sol::protected_function>()(owner);
        if (!fnResult.valid()) {
            sol::error err = fnResult;
            TraceLog(LOG_ERROR, "[GAME] RunScript error (%s:%s): %s",
                     scriptAsset.c_str(), functionName.c_str(), err.what());
            return false;
        }
    }
    TraceLog(LOG_INFO, "[GAME] Ran script '%s'%s", scriptAsset.c_str(),
             functionName.empty() ? "" : (" (" + functionName + ")").c_str());
    return true;
}
