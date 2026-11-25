-- Devilwood Lua Script: roter.lua
-- Available APIs: self, scene, vars, inventory, ui, sound, input, log(msg)
properties = {
    speed = 90.0,
    locked = true,
    message = "Hello",
    hotspot = ref("Hotspot")
}

---@param self Entity
function on_start(self)
    log("Started script on " .. self.name)
end

---@param self Entity
---@param dt number
function on_update(self, dt)
    if not properties.locked then
        self:set_rotation(self:get_rotation() + properties.speed * dt)
    end
end

---@param self Entity
function on_click(self)
    log("Clicked " .. self.name)
end
