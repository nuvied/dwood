-- Devilwood Lua Script: otherscript.lua
-- Available APIs: self, scene, vars, inventory, ui, sound, input, log(msg)
-- Expose Inspector properties using a top-level `properties` table:
-- properties = { speed = 90.0, active_on_start = true, message = "Hello" }

---@param self Entity
function on_start(self)
    log("Started script on " .. self.name)
end

---@param self Entity
---@param dt number
function on_update(self, dt)
end

---@param self Entity
function on_click(self)
    log("Clicked " .. self.name)
end
