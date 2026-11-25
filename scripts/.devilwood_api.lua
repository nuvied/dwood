---@meta
-- Auto-generated Devilwood Lua API definitions for VS Code (LuaLS)

---@class Entity
---@field name string
local Entity = {}
---@return boolean
function Entity:is_active() end
---@param active boolean
function Entity:set_active(active) end
---@return number
function Entity:get_x() end
---@return number
function Entity:get_y() end
---@param x number
---@param y number
function Entity:set_position(x, y) end
---@return number
function Entity:get_rotation() end
---@param degrees number
function Entity:set_rotation(degrees) end
---@return number
function Entity:get_scale_x() end
---@return number
function Entity:get_scale_y() end
---@param sx number
---@param sy number
function Entity:set_scale(sx, sy) end
---@return number
function Entity:get_alpha() end
---@param alpha number
function Entity:set_alpha(alpha) end
---@return string
function Entity:get_text() end
---@param text string
function Entity:set_text(text) end
---@param name string
---@return Entity|nil
function Entity:get_child(name) end
function Entity:trigger_interaction() end
---@return Entity|nil
function Entity:get_parent() end
---Get a component by type name: Transform, Sprite, Rectangle, Collider, CircleCollider,
---Text, Light, Audio, Interaction, Hotspot, InventoryItem, Script.
---@param type_name string
---@return any
function Entity:get_component(type_name) end
---@param type_name string
---@return boolean
function Entity:has_component(type_name) end
---@return TransformComponent|nil
function Entity:get_transform() end
---@return SpriteComponent|nil
function Entity:get_sprite() end
---@return RectangleComponent|nil
function Entity:get_rectangle() end
---@return ColliderComponent|nil
function Entity:get_collider() end
---@return CircleColliderComponent|nil
function Entity:get_circle_collider() end
---@return TextComponent|nil
function Entity:get_text_component() end
---@return LightComponent|nil
function Entity:get_light() end
---@return AudioComponent|nil
function Entity:get_audio() end
---@return InteractionComponent|nil
function Entity:get_interaction() end
---@return HotspotComponent|nil
function Entity:get_hotspot() end
---@return InventoryItemComponent|nil
function Entity:get_inventory_item() end
---@return ScriptComponent|nil
function Entity:get_script() end

---@class TransformComponent
---@field entity Entity
---@field x number
---@field y number
---@field rotation number
---@field scale_x number
---@field scale_y number
---@field world_x number readonly
---@field world_y number readonly
---@field world_rotation number readonly
local TransformComponent = {}
---@param x number
---@param y number
function TransformComponent:set_position(x, y) end
---@param dx number
---@param dy number
function TransformComponent:translate(dx, dy) end
---@param sx number
---@param sy? number
function TransformComponent:set_scale(sx, sy) end
---@param degrees number
function TransformComponent:rotate(degrees) end

---@class SpriteComponent
---@field entity Entity
---@field alpha number
---@field offset_x number
---@field offset_y number
---@field origin_x number
---@field origin_y number
---@field draw_order integer
---@field texture_width integer readonly
---@field texture_height integer readonly
local SpriteComponent = {}
---@return integer r, integer g, integer b, integer a
function SpriteComponent:get_color() end
---@param r integer
---@param g integer
---@param b integer
---@param a? integer
function SpriteComponent:set_color(r, g, b, a) end
---@return number x, number y, number w, number h
function SpriteComponent:get_source() end
---@param x number
---@param y number
---@param w number
---@param h number
function SpriteComponent:set_source(x, y, w, h) end
---@param x number
---@param y number
function SpriteComponent:set_origin(x, y) end

---@class RectangleComponent
---@field entity Entity
---@field width number
---@field height number
---@field opacity number
local RectangleComponent = {}
---@return integer r, integer g, integer b, integer a
function RectangleComponent:get_color() end
---@param r integer
---@param g integer
---@param b integer
---@param a? integer
function RectangleComponent:set_color(r, g, b, a) end

---@class ColliderComponent
---@field entity Entity
local ColliderComponent = {}
---@return number x, number y, number w, number h
function ColliderComponent:get_rect() end
---@param x number
---@param y number
---@return boolean
function ColliderComponent:contains(x, y) end

---@class CircleColliderComponent
---@field entity Entity
---@field radius number
local CircleColliderComponent = {}
---@return number x, number y, number radius
function CircleColliderComponent:get_circle() end
---@param x number
---@param y number
---@return boolean
function CircleColliderComponent:contains(x, y) end

---@class TextComponent
---@field entity Entity
---@field text string
---@field font string readonly
local TextComponent = {}
---@return number width, number height
function TextComponent:measure() end

---@class LightComponent
---@field entity Entity
---@field intensity number
---@field sprite_intensity number
---@field radius number
---@field use_sprite_shape boolean
---@field also_use_point_light boolean
local LightComponent = {}
---@return integer r, integer g, integer b, integer a
function LightComponent:get_color() end
---@param r integer
---@param g integer
---@param b integer
---@param a? integer
function LightComponent:set_color(r, g, b, a) end

---@class AudioComponent
---@field entity Entity
---@field asset string readonly
local AudioComponent = {}
function AudioComponent:play() end
function AudioComponent:pause() end
function AudioComponent:stop() end

---@class InteractionComponent
---@field entity Entity
---@field run_on_scene_start boolean
---@field action_count integer readonly
local InteractionComponent = {}
function InteractionComponent:trigger() end
---@return boolean
function InteractionComponent:is_running() end

---@class HotspotComponent
---@field entity Entity
---@field enabled boolean
---@field check_variable boolean
---@field variable_scope "global"|"scene"
---@field variable string
---@field require_item boolean
---@field required_item integer
---@field consume_item boolean
---@field disable_on_success boolean
---@field fail_message string
local HotspotComponent = {}
function HotspotComponent:trigger() end
---@param x number
---@param y number
---@return boolean
function HotspotComponent:contains(x, y) end

---@class InventoryItemComponent
---@field entity Entity
---@field item_id integer
local InventoryItemComponent = {}
---@return boolean
function InventoryItemComponent:collect() end
---@return boolean
function InventoryItemComponent:is_collecting() end

---@class ScriptComponent
---@field entity Entity
---@field script string readonly
local ScriptComponent = {}
---@return boolean
function ScriptComponent:is_loaded() end
---Read a global/property value from the other script.
---@param name string
---@return any
function ScriptComponent:get(name) end
---Write a value in the other script (also updates its `properties` table).
---@param name string
---@param value any
function ScriptComponent:set(name, value) end
---Call a function in the other script. Its owner entity is passed as the first argument (self).
---@param function_name string
---@param ... any
---@return any
function ScriptComponent:call(function_name, ...) end

---@param message string
function log(message) end

---Expose an object or component reference slot in the Inspector.
---@overload fun(): Entity|nil
---@overload fun(type_name: "Entity"): Entity|nil
---@overload fun(type_name: "Transform"): TransformComponent|nil
---@overload fun(type_name: "Sprite"): SpriteComponent|nil
---@overload fun(type_name: "Rectangle"): RectangleComponent|nil
---@overload fun(type_name: "Collider"): ColliderComponent|nil
---@overload fun(type_name: "CircleCollider"): CircleColliderComponent|nil
---@overload fun(type_name: "Text"): TextComponent|nil
---@overload fun(type_name: "Light"): LightComponent|nil
---@overload fun(type_name: "Audio"): AudioComponent|nil
---@overload fun(type_name: "Interaction"): InteractionComponent|nil
---@overload fun(type_name: "Hotspot"): HotspotComponent|nil
---@overload fun(type_name: "InventoryItem"): InventoryItemComponent|nil
---@overload fun(type_name: "Script"): ScriptComponent|nil
---@param type_name? string
---@return any
function ref(type_name) end

scene = {}
---@param name string
---@return Entity|nil
function scene.find(name) end
---@param scene_file string
function scene.change(scene_file) end
---@param scene_file string
---@param duration? number
function scene.open_popup(scene_file, duration) end
---@param duration? number
function scene.close_popup(duration) end

vars = {}
---@param name_or_id string
---@return boolean|integer|string|nil
function vars.get_global(name_or_id) end
---@param name_or_id string
---@param value boolean|integer|string
function vars.set_global(name_or_id, value) end
---@param name_or_id string
---@return boolean|integer|string|nil
function vars.get_scene(name_or_id) end
---@param name_or_id string
---@param value boolean|integer|string
function vars.set_scene(name_or_id, value) end

inventory = {}
---@param id integer
function inventory.add(id) end
---@param id integer
function inventory.remove(id) end
---@param id integer
---@return boolean
function inventory.has(id) end
---@return integer
function inventory.selected() end
---@param id integer
function inventory.select(id) end

ui = {}
---@param id string
function ui.show(id) end
---@param id string
function ui.hide(id) end
---@param id string
---@return boolean
function ui.is_visible(id) end
---@param id string
function ui.enable(id) end
---@param id string
function ui.disable(id) end
---@param id string
---@param duration? number
function ui.fade_in(id, duration) end
---@param id string
---@param duration? number
function ui.fade_out(id, duration) end
---@param text string
function ui.subtitle(text) end

sound = {}
---@param id integer
function sound.on(id) end
---@param id integer
function sound.off(id) end
---@param id integer
---@param duration? number
function sound.fade_in(id, duration) end
---@param id integer
---@param duration? number
function sound.fade_out(id, duration) end

input = {}
---@return number
function input.mouse_x() end
---@return number
function input.mouse_y() end
---@return boolean
function input.mouse_pressed() end
---@return boolean
function input.mouse_down() end
---@return boolean
function input.right_pressed() end

save = {}
---@param slot? integer
---@return boolean
function save.save(slot) end
---@param slot? integer
---@return boolean
function save.load(slot) end
---@param slot? integer
---@return boolean
function save.exists(slot) end
---@param slot? integer
---@return boolean
function save.delete(slot) end
