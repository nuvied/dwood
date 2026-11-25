# Devilwood

**Devilwood** is a 2D point-and-click adventure game engine and visual editor built with **Raylib**, **Dear ImGui**, and **Lua**. It provides a dockable scene editor, a visual node-based interaction editor, lighting, project-wide UI and subtitles, inventory & crafting, audio management, variables, save slots, and Lua scripting.

---

## Table of Contents

- [Quick Start: Your First Playable Scene](#quick-start-your-first-playable-scene)
- [Editor Workspace & Controls](#editor-workspace--controls)
- [Managing Projects & Settings](#managing-projects--settings)
- [Building Scenes](#building-scenes)
  - [Objects & Hierarchy](#objects--hierarchy)
  - [Components Reference](#components-reference)
  - [Lighting & Darkness](#lighting--darkness)
  - [Main Scenes vs. Popup Scenes](#main-scenes-vs-popup-scenes)
- [Interactions, Node Editor & Hotspots](#interactions-node-editor--hotspots)
  - [Using the Node Editor](#using-the-node-editor)
  - [Creating Hotspots & Conditions](#creating-hotspots--conditions)
  - [Supported Interaction Actions](#supported-interaction-actions)
- [UI, Subtitles, Inventory & Crafting](#ui-subtitles-inventory--crafting)
  - [Scene UI Layer vs. Project-Wide UI Manager](#scene-ui-layer-vs-project-wide-ui-manager)
  - [Subtitles](#subtitles)
  - [Inventory Items & Crafting](#inventory-items--crafting)
- [Audio & Sound Manager](#audio--sound-manager)
- [Variables & Save System](#variables--save-system)
- [Lua Scripting](#lua-scripting)
- [Building from Source](#building-from-source)
- [Project Structure & JSON Reference](#project-structure--json-reference)

---

## Quick Start: Your First Playable Scene

Follow these steps to go from an empty editor to a working interactive scene.

### 1. Create or Open a Project
1. Launch `editor` from your build directory.
2. Open **Project > New Project**, pick a destination folder and project name, and confirm. Devilwood copies the built-in `resource/` and `shaders/` templates and switches to your new project.
   - To open an existing project later, use **Project > Open Project** (select either the project folder or its `devilwood.project.json` file) or pick from **Project > Recent Projects**.

### 2. Create and Register a Scene
1. In the **Scenes** panel, type a scene ID (letters, numbers, `_`, or `-`, e.g. `cabin_interior`) and click **New scene**.
2. Set a friendly display name and click **Save**.
3. Open **Window > Scene Manager**, add your saved scene to the catalog, and click **Set startup**.
   - *(Optional)* Enable **Fade in at startup** for a smooth fade-in when the game launches.

### 3. Add Objects to the Scene
1. Click **Add object**, select the new object in the list, and click **Add component** to attach **Transform** and **Sprite**.
2. In the **Sprite** component, choose a PNG atlas from `resource/` and set its source rectangle in pixels.
   - Any new PNG placed in your project's `resource/` folder appears in the atlas selector automatically—no restart or rebuild needed.
3. Drag the object in the viewport to position it inside the **blue renderable frame**.
4. *(Optional)* Click **Add light** to place a point light, or **Add rectangle** / **Add UI** for quick shapes and text.

### 4. Add a Clickable Hotspot & Interaction
1. Click **Add interaction** to create an action list, select it, and click **Open node editor**.
2. Right-click empty graph space to add an action (for example, **Show subtitle** or **Change Scene**).
3. Click **Add hotspot** to create a clickable area, position and size its box collider over an object, and assign your interaction in the Inspector.

### 5. Play & Test
- Click **Play** to run the game directly inside the editor's **Game View**, and click **Stop** when done.
- Open **Window > Debug Log** to watch live gameplay events (picked items, hotspots, action lists, variables, scene changes) and inspect/edit live values in **Runtime Variables**.
- Click **Build & Run** to compile and launch the standalone `devilwood` runtime in a separate window.

> **Note:** Both **Play** and **Build & Run** automatically save the open scene and pending variable edits first (a save error prevents launching). They start from the configured startup scene—or the active scene if no startup scene is set.

---

## Editor Workspace & Controls

| Action | Control |
| --- | --- |
| Select object | Left-click in viewport or **Object List** |
| Move object | Drag object in viewport |
| Pan scene viewport | Middle-mouse drag |
| Zoom scene viewport | Mouse wheel |
| Adjust numeric fields | Drag horizontally on the field, or click to type |
| Undo / Redo scene edits | `Ctrl+Z` / `Ctrl+Y` (or `Ctrl+Shift+Z`) |
| Dock, split, or tab panels | Drag any panel's title bar |

### Useful Panels (`Window` Menu)
- **Window > Object List** — Browse, select, and manage objects in the current scene independently of the Scenes panel.
- **Window > Scene Manager** — Register scenes in the runtime catalog and set the startup scene.
- **Window > Project Settings** — Configure project resolution (**Project** tab) and scene darkness/type (**Scene Settings** tab).
- **Window > UI Manager** — Build project-wide UI overlays, subtitles, inventory, and crafting panels.
- **Window > Sound Manager** — Manage project-wide BGM and SFX tracks.
- **Window > Variable Manager** — Define Global and Scene variables.
- **Window > Runtime Variables** — View and edit live variable values during **Play** without modifying saved defaults (opens automatically on Play).
- **Window > Recipe Editor** — Create crafting recipes from saved inventory items.
- **Window > Debug Log** — Live gameplay and engine logs with **Game** and **Engine** filter toggles.

Window positions, sizes, visibility, docking layout, and viewport zoom/pan are saved per project inside `.devilwood/` and restored automatically on reopen.

---

## Managing Projects & Settings

### Project Menu & CLI
- **Project > New Project**: Creates a project folder with `devilwood.project.json`, `resource/`, and `shaders/`.
- **Project > Open Project**: Accepts a project folder or `devilwood.project.json` (on Windows, **Browse...** opens a folder picker).
- **Project > Recent Projects**: Lists up to 10 recent projects (stored in the user's app config directory, not inside the project).
- On launch, the editor reopens the last project (falling back to the built-in project if unavailable). You can also pass a project folder, `devilwood.project.json`, or scenes directory as the editor's first command-line argument.
- Switching projects automatically saves the current scene, scene/global variables, UI, inventory, and project settings.

### Resolution & Render Settings
Open **Window > Project Settings > Project**:
- Set the project-wide render **width** and **height** (default: `1024 × 512`) and click **Apply and save**.
- The game renders internally at **2×** the configured resolution and displays it with point filtering for a crisp look.
- The **blue frame** in the scene viewport marks the renderable area; objects outside it remain editable but are clipped at runtime.

---

## Building Scenes

Scenes are saved as JSON files in your project's `scenes/` directory so they can be version-controlled and shared with a team. Unsaved changes prompt for confirmation before switching scenes or closing the editor.

### Objects & Hierarchy
- Every object in a scene can be parented to another object (the editor displays indented hierarchy entries and prevents cycles).
- Child **Transform** positions are local to the parent; child sprites and colliders inherit parent position, rotation, and scale.
- Removing a parent promotes its direct children to root objects while preserving their other links.

### Components Reference

Click **Add object** and use **Add component** in the Inspector (or use the quick-add buttons in the scene panel):

| Component / Quick-Add | Purpose & Key Properties |
| --- | --- |
| **Transform** | Position, rotation, and positive X/Y scale (`1` = original size). A Sprite without Transform defaults to the origin. |
| **Sprite** | Renders a pixel rectangle (`source`) from a PNG atlas in `resource/`. Supports `render_order` (lower draws first), local `offset`, and `opacity` (`0`–`1`). |
| **Panel Sprite** | Nine-patch scalable sprite with `margin`, `width`, and `height`. Automatically adds a default Sprite if missing. |
| **Rectangle** (**Add rectangle**) | Solid filled rectangle with `width`, `height`, RGBA `color`, `opacity` (`0`–`1`), and `render_order`. Follows Transform. |
| **Box / Circle Collider** | Rectangular (`width`, `height`) or circular (`radius`, optional `x`/`y`) bounds. Sprite-backed box colliders follow the Sprite or Panel size. |
| **Text** | Displays text with `size`, RGBA `color`, `margin` offset, and optional custom font. Place `.ttf`/`.otf` files in `resource/fonts/` and rebuild (click **Refresh fonts** to retry). Uses Raylib's basic ASCII character set. |
| **Light** (**Add light**) | Point light or additive sprite-shaped light (see [Lighting & Darkness](#lighting--darkness)). |
| **Audio** | Scene-attached audio using a path relative to `resource/`, with `volume` and **Play on start**. Controlled via `PlayAudio`, `PauseAudio`, or `StopAudio`. |
| **Inventory Item** (**Add inventory item**) | Collectible item linked to an ID in `.devilwood/inventory_items.json` (requires a Sprite). Clicking it at runtime adds the item to the player's inventory and hides the object. |
| **Interaction** (**Add interaction**) | Linear list of gameplay actions edited in the Inspector or Node Editor. |
| **Hotspot** (**Add hotspot**) | Editor-only clickable rectangle (requires Transform + Box Collider) that triggers an Interaction on click. |
| **Script** | Attaches a `.lua` script from `scripts/` with per-object Inspector property overrides. |

### Lighting & Darkness
1. Adjust the scene's ambient darkness in **Window > Project Settings > Scene Settings** from `0` (brightest) to `1` (darkest; default `0.65`) and save the scene.
2. Use **Add light** to place a radial point light and configure its **radius**, **intensity**, and **color**.
   - Up to **7 radial scene lights** are supported per scene (plus the gameplay light slot).
   - Toggle **Preview lighting** in the scene view to inspect the runtime lighting effect.
3. **Sprite Lights**: Add a **Sprite** component to a light and enable **Use Sprite as shape** to additively composite the sprite's alpha shape with its own **sprite intensity**. Enable **Also use point light** to combine both with independent intensities.

### Main Scenes vs. Popup Scenes
In **Window > Project Settings > Scene Settings**, set a scene's type to **Main** or **Popup**:
- **Main scenes** are full scenes transitioned to with **Change Scene** (or `Game::ChangeSceneF(scenePath)` in C++).
- **Popup scenes** overlay a parent Main scene (great for close-up puzzles, notes, or locks):
  - Set the popup's **parent** Main scene in Scene Settings; popups appear nested under their parent in the Scene List.
  - Register the parent in **Scene Manager** before registering its popup. Popups cannot be startup scenes, and a parent cannot be unregistered while its popups remain registered.
  - Open a popup with **Open Popup Scene** and dismiss it with **Close Popup Scene**. Each action accepts a fade **duration** in seconds (default `0.5`; `0` for instant).
  - While a popup is open, it renders above its parent, only the top scene receives updates and hotspot clicks, the parent's **Scene variables** are suspended and restored on close, and **Global variables** remain shared.

---

## Interactions, Node Editor & Hotspots

### Using the Node Editor
1. Select an object with an **Interaction** component and click **Open node editor**.
2. **Right-click empty space** to add an action from the **Category** and **Action** menus.
3. **Right-click a node** to delete it.
4. **Drag nodes** to organize them, **right-mouse drag** to pan, **mouse wheel** to zoom, and click **Fit graph** to reset the view.
5. Node positions are saved per interaction. Links follow the action order automatically (the graph is linear; manual rewiring is not used). You can also edit actions directly in the collapsible Inspector list.

> **Tip:** Place UI, audio, or variable actions **before** a **Change Scene** action—changing scenes immediately ends the current interaction.

### Creating Hotspots & Conditions
1. Click **Add hotspot**, resize its Box Collider, and select the target **Interaction** in the Inspector.
2. Clicks on a hotspot are ignored while its interaction is already running.
3. Optional hotspot features:
   - **Conditional gating**: Enable **Check variable** (Global or Scene variable) and/or **Require item** (with optional **Consume item**). If a condition fails, the hotspot displays **Fail message** as a subtitle instead of running the interaction.
   - **Disable on success**: Automatically disables the hotspot after it succeeds.
   - **Item interactions**: Add `item_interactions` entries to run different interactions depending on which inventory item the player currently has selected (each entry supports its own `consume_item` and `disable_on_success`).

### Supported Interaction Actions

| Category | Actions |
| --- | --- |
| **Scene** | `ChangeScene` (Main scenes only), `OpenPopupScene`, `ClosePopupScene` |
| **UI & Subtitles** | `ShowUI`, `HideUI`, `EnableUI`, `DisableUI`, `FadeInUI` (group), `FadeOutUI` (group), `ShowSubtitle` |
| **Variables & Logic** | `SetVariable` (`global` or `scene`), `IfElse` (`==`, `!=` for all types; `<`, `<=`, `>`, `>=` for ints) |
| **Objects & Hotspots** | `ShowObject`, `HideObject`, `DisableHotspot`, `ChangeHotspotSetting` |
| **Audio** | Scene audio: `PlayAudio`, `PauseAudio`, `StopAudio` · Sound Manager: `SoundOn`, `SoundOff`, `SoundFadeIn`, `SoundFadeOut` |
| **Save System** | `SaveGame`, `LoadGame`, `DeleteSave` (slots `1`–`99`) |
| **Scripting** | `RunScript` (`script` filename and optional `function` name) |

---

## UI, Subtitles, Inventory & Crafting

### Scene UI Layer vs. Project-Wide UI Manager
- **Scene UI Layer**: Click **Add UI** or check **UI layer** on any scene object. UI objects and their descendants render above the lit scene using the UI camera (unaffected by scene lights).
- **Project-Wide UI Manager**: Open **Window > UI Manager** and click **Create UI Manager** for persistent UI shared across all scenes (saved in `scenes/.devilwood/project_ui.json`).
  - Uses the game's `512 × 256` authored coordinate space (scaled to the configured resolution in the preview).
  - Use **Add Group** to create top-level containers (e.g., HUD, dialogue, inventory). Groups cannot be nested, but elements inside them can have children. Use the **Parent** selector to reparent without shifting visual position.
  - **Visible** controls rendering; **Enabled** controls updates and input independently (hidden elements can still receive clicks unless disabled). Both cascade to descendants.
  - **Alpha** (`0`–`1`) multiplies down the hierarchy (and with Text color alpha).
  - **Block scene clicks** prevents clicks on the UI element from passing through to scene hotspots.
  - UI runtime state persists across scene changes and resets to its saved initial state when **Play** restarts.

### Subtitles
1. In **Window > UI Manager**, click **Add Subtitle** to create an initially hidden subtitle group with a child Text element.
2. Select which child UI Text receives the subtitle text and set **Type-in speed** (characters per second, default `30`).
3. Trigger subtitles from any interaction using **Show subtitle** (or `ui.subtitle("...")` in Lua).
   - The text types in as the group fades in, stays visible for `0.15s × character count`, and fades out over `0.25s`.
   - Selecting the subtitle group or its child in the UI Manager temporarily reveals it in the preview so you can position it.

### Inventory Items & Crafting
- **Define items**: Manage project inventory items in `.devilwood/inventory_items.json` (via the Inventory Item Editor) and place pickups in scenes with **Add inventory item**.
- **Inventory UI**: Clicking a slot in a project UI `InventoryComp` selects or deselects that item (right-click also deselects).
- **Crafting Panel**:
  1. In **Window > UI Manager**, select a group and click **Add Crafting Panel** to add the 3-input, 1-output crafting UI.
  2. Open **Window > Recipe Editor** and create recipes using **1 to 3 distinct inventory items** (order-independent). Recipes are saved in `.devilwood/crafting_recipes.json`.
  3. At runtime, select an item in the inventory and click an input slot (click a slot with no item selected to clear it). When a recipe matches, clicking the output slot consumes the inputs and adds the result (unless the result is already owned and not one of the inputs).

---

## Audio & Sound Manager

- **Scene Audio (`AudioComp`)**: Attached to a scene object with a resource-relative path, volume, and **Play on start**. Best for scene-local sounds.
- **Project-Wide Sound Manager (`Window > Sound Manager`)**: Manages BGM and SFX across scenes (saved in `.devilwood/sounds.json`).
  - Automatically lists audio files placed in `resource/sounds/`.
  - Only **one BGM** plays at a time (supports looping); **SFX** can overlap.
  - Control managed sounds from interactions using `SoundOn`, `SoundOff`, `SoundFadeIn`, and `SoundFadeOut` (or via the `sound` Lua API).

---

## Variables & Save System

### Variables (`Window > Variable Manager`)
- **Global variables** are shared across all scenes (`.devilwood/global_variables.json`) and reset to defaults when **Play** starts.
- **Scene variables** belong to the open scene (saved in the scene JSON) and reset to defaults whenever that scene loads.
- Supported types: `bool`, `int`, and `string`.
- During **Play**, use **Window > Runtime Variables** to inspect and tweak live values without changing saved defaults.

### Save Slots
- Use `SaveGame`, `LoadGame`, and `DeleteSave` actions (or the `save` Lua API) with slot numbers `1`–`99`.
- Saves are stored in `.devilwood/saves/slot_<N>.json` and capture the active scene, global/scene variables, runtime inventory, and picked items.

---

## Lua Scripting

Devilwood embeds **Lua + sol2** for custom gameplay logic. Place `.lua` files in your project's `scripts/` folder and attach them via a **Script** component (`ScriptComp`), or trigger them from an interaction using **RunScript**.

### Example Script (`scripts/roter.lua`)

```lua
-- Expose editable fields in the Inspector (bool, number, string, or ref())
properties = {
    speed = 90.0,
    locked = true,
    message = "Hello",
    hotspot = ref("Hotspot") -- Drag/assign an object with a Hotspot component in the Inspector
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
```

### Script Capabilities
- **Lifecycle callbacks**: `on_start(self)`, `on_update(self, dt)`, and `on_click(self)`.
- **Inspector properties**: Declare a top-level `properties = { ... }` table with booleans, numbers, strings, or `ref("ComponentType")` slots. Per-object overrides are injected into each object's isolated Lua environment before `on_start(self)` runs.
- **Built-in globals**:
  - `self` — The owning `Entity` (position, rotation, scale, alpha, text, parent/children, `get_component(...)`, `trigger_interaction()`).
  - `scene` — `find(name)`, `change(file)`, `open_popup(file, duration)`, `close_popup(duration)`.
  - `vars` — `get_global`, `set_global`, `get_scene`, `set_scene`.
  - `inventory` — `add`, `remove`, `has`, `selected`, `select`.
  - `ui` — `show`, `hide`, `is_visible`, `enable`, `disable`, `fade_in`, `fade_out`, `subtitle`.
  - `sound` — `on`, `off`, `fade_in`, `fade_out`.
  - `input` — `mouse_x`, `mouse_y`, `mouse_pressed`, `mouse_down`, `right_pressed`.
  - `save` — `save(slot)`, `load(slot)`, `exists(slot)`, `delete(slot)`.
  - `log(msg)` — Prints to the editor's **Debug Log**.
- **VS Code Autocomplete**: The project includes `scripts/.devilwood_api.lua` and `scripts/.luarc.json` for LuaLS IntelliSense.

---

## Building from Source

### Dependencies (via vcpkg)
Install the following packages (example for `x64-windows`):

```powershell
vcpkg install raylib:x64-windows glfw3:x64-windows "imgui[docking-experimental]:x64-windows" imgui-node-editor:x64-windows nlohmann-json:x64-windows lua:x64-windows sol2:x64-windows
```

- **Dear ImGui**: Requires **1.92+** with the docking branch (`imgui[docking-experimental]`).
- **imgui-node-editor**: Linked only to the `editor` target (tested with `0.9.3#4`).

### Configure & Build

```powershell
cmake -B out/build -S . -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build out/build
```

- **Working directory**: Both `editor` and `devilwood` expect `resource/` and `shaders/` in their working directory. CMake automatically syncs changed assets into the build directory—run both executables from there.
  - If a modified asset is locked by a running instance, close the game or editor before rebuilding.
- **Raylib Custom Frame Control**: If your Raylib build uses `SUPPORT_CUSTOM_FRAME_CONTROL`, `DEVILWOOD_CUSTOM_FRAME_CONTROL` must match it. It defaults to `ON` for Raylib 6+ with vcpkg and `OFF` otherwise. Override if needed:
  ```powershell
  cmake -B out/build -S . -DDEVILWOOD_CUSTOM_FRAME_CONTROL=ON
  ```

---

## Project Structure & JSON Reference

### Project Layout

```text
<project-root>/
├── devilwood.project.json         # Project manifest
├── resource/                      # PNG atlases, fonts/ (.ttf, .otf), sounds/
├── shaders/                       # Lighting and post-processing shaders
├── scripts/                       # Lua scripts + .devilwood_api.lua
└── scenes/
    ├── main_scene.json            # Scene files
    └── .devilwood/
        ├── scene_catalog.json     # Registered scenes & startup scene
        ├── game_settings.json     # Render resolution
        ├── project_ui.json        # Project-wide UI & subtitles
        ├── global_variables.json  # Global variable definitions
        ├── sounds.json            # Sound Manager BGM/SFX entries
        ├── inventory_items.json   # Inventory item definitions
        ├── crafting_recipes.json  # Crafting recipes
        ├── saves/slot_<N>.json    # Runtime save slots (1–99)
        └── *.json / *.ini         # Per-project editor layout & settings
```

<details>
<summary><strong>Scene & Component JSON Schema Reference</strong> (click to expand)</summary>

#### Scene Root
```json
{
  "version": 1,
  "name": "Main Scene",
  "settings": {
    "darkness": 0.65,
    "type": "main"
  },
  "variables": [],
  "objects": []
}
```
- `settings.darkness`: `0` (brightest) to `1` (darkest); defaults to `0.65`.
- `settings.type`: `"main"` or `"popup"` (defaults to `"main"`). Popup scenes also store `"parent": "parent-scene.json"`.
- Each object has `"name"`, `"components"`, optional `"parent"` (0-based index in `objects`), and optional `"ui": true`.
- Unsupported object data is preserved on save; runtime scenes load only supported components (`TransformComp`, `Sprite`, `Panel_Sprite`, `RectangleComp`, `ColliderComp`, `CircleCol`, `TextComp`, `LightComp`, `AudioComp`, `HotspotComp`, `InteractionComp`, `InventoryItemComp`, `ScriptComp`). Older legacy sprite objects remain readable and convert to components when edited (missing scale defaults to `1, 1`).

#### Components
- **`TransformComp`**: `position` (`x`, `y`), `scale` (`x`, `y`), optional `rotation`.
- **`Sprite`**: `atlas` (PNG filename), `source` (`x`, `y`, `width`, `height`), optional `render_order` (default `0`), optional `offset` (`x`, `y`), optional `opacity` (`0`–`1`).
- **`Panel_Sprite`**: `margin`, `width`, `height` (requires `Sprite`).
- **`RectangleComp`**: `width`, `height`, RGBA `color`, `opacity` (`0`–`1`), optional `render_order` (default `0`).
  ```json
  "RectangleComp": {"width": 128, "height": 64, "color": {"r": 80, "g": 160, "b": 255, "a": 255}, "opacity": 0.75}
  ```
- **`ColliderComp`**: `width`, `height`.
- **`CircleCol`**: `radius`, optional `x`, `y`.
- **`TextComp`**: `text`, `size`, optional RGBA `color` (`0`–`255`), optional `margin` offset, optional `font` (`.ttf`/`.otf` in `resource/fonts`).
- **`LightComp`**: `intensity`, `radius`, optional RGBA `color`, optional `use_sprite_shape` (default `false`, requires `Sprite`), optional `sprite_intensity` (defaults to `intensity`), optional `also_use_point_light` (default `false`).
- **`AudioComp`**: resource-relative path, `volume`, `play_on_start`.
- **`InventoryItemComp`**: integer `item` ID (requires `Sprite`).
- **`ScriptComp`**: `.lua` filename in `scripts/` and optional `properties` overrides.
- **`HotspotComp`**: `interaction` (object index or `null`; requires `TransformComp` and `ColliderComp`), optional `item_interactions` (`{"item": -1, "interaction": 0, "consume_item": false, "disable_on_success": false}`), optional `check_variable` + `variable` (`scope`, `id`), `require_item` + `item` + `consume_item`, `disable_on_success`, and `fail_message`.

#### Interaction Actions (`InteractionComp.actions`)
- Scene: `{"type":"ChangeScene","scene":"scene-id.json"}`, `{"type":"OpenPopupScene","scene":"popup-id.json","duration":0.5}`, `{"type":"ClosePopupScene","duration":0.5}`
- UI: `ShowUI`, `HideUI`, `EnableUI`, `DisableUI` (`"ui": "element-id"`), `FadeInUI` / `FadeOutUI` (group ID + `"duration"`), `{"type":"ShowSubtitle","text":"A message"}`
  - Subtitle groups in `project_ui.json` use `"type":"subtitle"`, `"group":true`, `"subtitle_text":"child-ui-id"`, and optional `"type_in_speed":30`.
  - Project UI elements use stable `id` values, optional `visible`, `enabled`, `alpha` (default `1`), `"group": true`, and 0-based `parent` index.
- Variables & Flow: `{"type":"SetVariable","scope":"global","id":"variable_id","value":true}` (`scope`: `"global"` or `"scene"`), `IfElse` (`condition`: `{scope, id, operator, value}`, `then`: `[...]`, `else`: `[...]`).
- Audio: `PlayAudio` / `PauseAudio` / `StopAudio` (`"audio": <index>`), `SoundOn` / `SoundOff` / `SoundFadeIn` / `SoundFadeOut` (`"sound": <id>`, optional `"duration"`).
- Objects & Hotspots: `ShowObject` / `HideObject` (`"object": <index>`), `DisableHotspot` (`"hotspot": <index>`), `ChangeHotspotSetting` (`"hotspot": <index>` + updated fields).
- Saves: `SaveGame` / `LoadGame` / `DeleteSave` (`"slot": 1..99`).
- Scripts: `RunScript` (`"script": "file.lua"`, optional `"function": "fn_name"`).

</details>
