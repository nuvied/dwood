# Devilwood

The `devilwood` and `editor` targets expect `resource` and `shaders` in their
working directory. CMake copies changed assets to the build directory when
building the editor or game, so run both programs from that directory. If a
changed asset is in use, close the game or editor before rebuilding. The editor uses
Dear ImGui 1.92 or newer with its docking branch, imgui-node-editor, and
nlohmann-json (available as `imgui`, `imgui-node-editor`, and `nlohmann-json`
in vcpkg). Enable vcpkg's `imgui[docking-experimental]` feature; for example,
install `imgui[docking-experimental]:x64-windows` for the x64 Windows triplet.
The node editor is linked only to the editor target; vcpkg's
imgui-node-editor 0.9.3#4 has been tested.

Use the editor's **Project** menu to create, open, or save projects. **New Project**
asks for a destination folder and project name, copies the built-in resource and
shader templates, and switches to the new project. **Open Project** accepts either
a project folder or its `devilwood.project.json` path; on Windows, **Browse...**
opens a folder picker. The **Recent Projects** menu lists up to ten projects.
The editor reopens the last project on launch and falls back to the built-in project
if that project is no longer available. Project history is stored in the user's
application configuration directory, not in the project. Project switches save
the current scene, its variables, Global variables, UI, inventory, and project settings first. You can also pass a
project folder or project-file path as the editor's first command-line argument.

The editor opens in the Scenes workspace. Enter a scene ID using letters,
numbers, `_` or `-`, select **New scene**, edit its display name, and select

**Save**. Select a JSON file in the list to reopen it. Files are saved under
the source project's `scenes` directory so they can be shared with the team;
pass a directory as the editor's first argument to use another location.
Unsaved changes are confirmed before switching scenes or closing the editor.
Use **Play** to run the game in the editor and **Stop** to return to scene
editing. **Build & Run** builds the runtime and opens it in a separate window.
Both actions save the open scene and pending variable definitions first; a save error prevents the run.
They still launch the configured startup scene, which can differ from the open scene.
Both run buttons change to Stop controls while their game is running. Use the
**Window** menu to show or hide editor panels, including **Window > Debug Log**
for live gameplay logs (picked items, action lists, scene changes, hotspots,
variables) with options to toggle **Game** and **Engine** logs. Use **Window > Object List** to
view, select, and manage the current scene's objects independently of the
Scenes panel. Drag window title bars to dock, split, or tab editor windows;
the docking layout is saved with the project's other window settings.
Use **Ctrl+Z** to undo and **Ctrl+Y** or **Ctrl+Shift+Z** to redo scene edits.
Window positions, sizes, visibility, and scene-canvas zoom/pan are saved per
project in its `.devilwood` settings directory and restored when the project
opens.

Use **Add object**, select it, then use **Add component** to add Transform,
Sprite, panel sprite, box or circle collider, text, or audio. Audio assets use
paths relative to `resource/`; set volume and **Play on start** in the Audio
component. Interaction actions can play, pause, or stop a selected Audio object.
Use **Window > Sound Manager** for project-wide sounds that are not attached to
scene objects. Add BGM or SFX entries with resource-relative audio paths,
volume, and (for BGM) looping. Audio files in `resource/sounds` are listed
automatically and can be selected when adding or editing entries. Only one BGM
plays at a time; SFX can overlap.
Interactions can turn a managed sound on/off or fade it in/out. Manager settings
are saved in `.devilwood/sounds.json` inside the configured scenes directory.
Use **Window > Variable Manager** to add Global variables shared by every scene,
or Scene variables saved with the open scene. Variables support bool, int, and
string defaults. They reset to their defaults when Play starts (Global) or when
a scene loads (Scene). The manager's Save buttons persist Global definitions in
`.devilwood/global_variables.json` and Scene definitions in the scene JSON file;
Play and Build & Run also save pending variable edits before starting. During
Play in the editor, the Runtime Variables window shows and lets you edit live
variable values directly without changing saved defaults; it opens automatically
and can be reopened from the Window menu.
Transform controls
position, rotation, and scale (positive X and Y multipliers; 1 means original
size). Sprite selects the atlas and its rectangle in pixels. PNG textures added
to the project `resource` directory while the editor is open appear in atlas
selectors and load when selected; no rebuild or restart is needed. Panel sprites use
a nine-patch margin and a target size; adding one also adds a default Sprite
if needed. Text supports content, font type, font size, color, and offset. To use a custom
TrueType (`.ttf`) or OpenType (`.otf`) font, place it in `resource/fonts` in
the project and build the editor or game. Select the font in the Text component;
**Refresh fonts** retries missing or invalid files. Existing scenes use Raylib's
default font unless a custom font is selected. Custom fonts use Raylib's default
character set (basic ASCII); characters outside it are not loaded. Drag objects
inside the scene viewport to reposition them; clicking an object selects it.
Pan the scene with the middle mouse button and zoom with the mouse wheel.
The scene-editing workspace fills the resizable editor window. A blue frame
marks the renderable area; objects outside it remain editable but are clipped
from the scene view. Open **Window > Project Settings > Project** to set the
project-wide render width and height (default 1024x512), then select **Apply and save** to
update the editor and runtime render resolution. The configured dimensions are
the Game View/output size; the game renders internally at twice that resolution
and uses point filtering when displayed for a crisp result. Settings are stored in
`scenes/.devilwood/game_settings.json` (or in the selected scene directory).
Use the **Scene Settings** tab in the same window to adjust the current scene's
base darkness from 0 (brightest) to 1 (darkest), or set its type to Main or Popup;
save the scene to persist these settings. A popup scene must name a Main scene as
its parent. In the Scene List, popup scenes appear under their parent scene's
foldout. Register the parent before registering its popup in Scene Manager;
popup scenes cannot be startup scenes, and a parent cannot be unregistered while
its popups remain registered.
Open **Window > Scene Manager** to add the saved current scene to the runtime
game catalog and load it for Play in Editor. Registered scenes are stored in
`scenes/.devilwood/scene_catalog.json`; **Set startup** selects the JSON scene
loaded by the game on launch. Enable **Fade in at startup** to fade into that
scene when the game starts or when Play is pressed. The in-editor **Game View**
fits the configured render target to its resizable window. Game code can
transition to a registered JSON scene with `Game::ChangeSceneF(scenePath)`.
Use an **Open Popup Scene** interaction to overlay a registered popup belonging
to the active main scene. Opening fades to black, shows the popup, then fades
back in. **Open Popup Scene** and **Close Popup Scene** each let you set a
non-negative duration per fade in seconds (default 0.5; zero makes the fades
instant). Closing fades out, dismisses the top popup, then fades back in.
Popup scenes draw above their parent and only the top scene receives updates and
hotspot input. Scene-scoped variable values are suspended while a popup is open
and restored when it closes; Global variables remain shared. **Change Scene**
continues to replace the active scene and accepts only Main scenes.
Numeric fields use draggable controls: drag a value horizontally to adjust it,
or enter a value directly for precise edits.
Game View runs game `Entity` and component instances and uses their
drawing behavior, with collider outlines shown for editing. A Sprite without
Transform uses the game's default position at the origin. The atlas rectangle
must fit the chosen image. The atlas picker lists the four PNG resources
currently loaded by the game. Components and objects can be removed in the
inspector. Atlas rectangle selection by drawing a box is not implemented yet.
Choose **Add rectangle** in the scene panel to create a filled rectangle object.
Set its size, RGBA color, opacity, render order, and Transform in the inspector;
lower render-order values draw first and higher values draw on top. The rectangle
follows its Transform position, scale, and rotation.
Use **Add light** in the scene panel to create a point-light object, then adjust
its scene-unit radius, intensity, and color in the inspector. Add a Sprite to a
light and enable **Use Sprite as shape** to additively composite the sprite's
alpha shape over the scene. The sprite light has its own intensity; enable
**Also use point light** to combine it with the radial light and set each
intensity separately. The radial light shader supports up to seven scene lights
per scene, in addition to the gameplay light slot; the scene view shows each
radial light's radius gizmo. **Preview lighting**
shows the runtime light effect in the scene view and can be turned off.

Use **Add UI** to create a UI text object, or enable **UI layer** on an object.
UI descendants inherit the UI layer. UI objects draw over the lit scene using
the UI camera, so scene lights do not affect them. Add Sprite, panel sprite, or
Text components to build the UI.

Open **Window > UI Manager** and choose **Create UI Manager** to add an
optional project-wide UI shared across scenes. Add text, sprites, or panels,
select an element, and edit its properties or drag it in the game-sized preview.
Use **Add Subtitle** to create an initially hidden subtitle group with a child
text object. In the group's properties, select which child UI Text object
receives subtitle text. Set the group's type-in speed in characters per second;
an interaction supplies the text, which types in as the group fades in. After
typing, it stays visible for 0.15 seconds per character, then fades out over
0.25 seconds. The selected subtitle group or child is shown in the editor
preview so it can be positioned.
Use **Add Group** for separate root containers such as inventory or dialogue.
Select a group or element before adding an element to make it a child, or use
the **Parent** selector to reparent it without moving it in the preview.
Groups cannot be nested, but their elements can have children. Existing
ungrouped elements remain supported. Removing a parent makes its direct children
roots, retaining their local transforms.
Positions use the same 512 x 256 authored coordinate space as the game; the
preview scales it to the configured game resolution. Elements draw above the
scene without lighting, alongside the existing built-in inventory UI. **Visible**
controls drawing; **Enabled** controls updates and input independently. Hidden
elements can still receive input unless disabled. These states also apply to
descendants without changing their individual settings. Each element and group
has an alpha from 0 to 1; descendant alpha is multiplied by its ancestors'
alpha and, for text, its color alpha. **Block scene clicks** adds a click area.
UI changes are saved before Play, Build & Run, or editor exit.
The manager is stored in `scenes/.devilwood/project_ui.json` (or the selected
scene directory), and its runtime state persists across scene changes. Starting
Play again reloads its saved initial state.

Use **Add interaction** to create an action list. Select it and open **Open node
editor** to view its actions as a linear graph. Right-click empty graph space
to add an action from the category and action menus; right-click an action node
to delete it. Node positions are saved per interaction. Drag nodes to arrange
them, pan with the right mouse button, and zoom with the mouse wheel; **Fit
graph** resets the view. Links follow the action order automatically; branching
and manual connections are not supported. The action list in the inspector can
be collapsed, and each action is outlined to distinguish it from other
controls. The action inspector remains available as well.
Use the **Category** and **Action** menus to add UI actions that show, hide,
enable, or disable a project UI element or group, or fade a group in or out over
a chosen duration. Fade-in shows the
group and restores its configured alpha; fade-out hides it when finished.
**Show subtitle** takes a text string and displays it in the child Text component
of the Subtitle group, using its type-in speed and automatically fading it out
based on the number of characters. The Subtitle group is configured in the UI
Manager; the action does not select a UI element.
Fading does not change enabled state, and later actions wait for the fade.
Runtime UI actions safely skip when the project has no UI manager or the
referenced element is missing. Add actions before Change Scene, which ends the
current interaction. Deleting a UI element leaves any
existing references unresolved so they can be corrected in the editor.
Use **Add hotspot** to create a
clickable rectangle, adjust its Transform and box collider, and select the
interaction in its inspector. The rectangle is visible only in the editor.
Clicking the hotspot runs the interaction; clicks while it is running are
ignored. A scene change uses the existing fade transition and ends the current
scene's interaction. Register the target scene before triggering the action.

Scene files use `{"version": 1, "name": "...", "settings": {"darkness": 0.65, "type": "main"}, "objects": []}`.
Scene `settings.darkness` controls base darkness from `0` (brightest) to `1`
(darkest); older scenes without it use `0.65`. `settings.type` is `main` or
`popup` (older scenes without it are Main); a popup also stores its parent's
scene filename in `settings.parent`. A sprite object has a `name`
and a `components` object. `TransformComp` stores
`position`, `scale`, and optionally `rotation`; `Sprite` stores an `atlas`
PNG filename and a `source` rectangle with `x`, `y`, `width`, and `height`.
Sprites may also specify an integer `render_order` (default `0`); lower values
draw first and higher values draw on top. An optional `offset` with `x` and `y`
values positions the sprite in local Transform units, following its scale and
rotation. Optional `opacity` ranges from `0` (transparent) to `1` (opaque).
`LightComp` stores `intensity`,
`radius`, optional RGBA `color`, and optional boolean `use_sprite_shape`
(default `false`, requires a `Sprite`). Sprite lights may set `sprite_intensity`
(defaults to `intensity` for existing scenes) and `also_use_point_light`
(default `false`, requires `use_sprite_shape`) to combine separate sprite and
radial light intensities.
`Panel_Sprite` stores `margin`, `width`, and `height` and requires a `Sprite`.
`ColliderComp` stores `width` and `height` (sprite-backed colliders follow the
Sprite or panel size); `CircleCol` stores `radius` and optional `x` and `y`.
`TextComp` stores `text`, `size`, optional RGBA `color` channels (0-255), and
optional `margin` offset. Optional `font` is a `.ttf` or `.otf` filename in
`resource/fonts`; omit it or use an empty string for Raylib's default font.
`InteractionComp` stores an `actions` array; Change Scene entries use
`{"type":"ChangeScene","scene":"scene-id.json"}` and UI visibility entries use
`{"type":"ShowUI","ui":"element-id"}` or `{"type":"HideUI","ui":"element-id"}`.
Popup interactions use `{"type":"OpenPopupScene","scene":"popup-id.json","duration":0.5}` or
`{"type":"ClosePopupScene","duration":0.5}`. The optional non-negative `duration`
applies to each fade and defaults to 0.5 seconds.
Project UI subtitle groups use `"type":"subtitle"`, `"group":true`,
`"subtitle_text":"child-ui-id"` to select a child UI object with `TextComp`, and
optional `"type_in_speed":30` (characters per second, default 30). Show one with
`{"type":"ShowSubtitle","text":"A message"}`.
Variable actions use `{"type":"SetVariable","scope":"global","id":"variable_id","value":true}`;
the scope is explicitly `global` or `scene`. An `IfElse` action has a `condition`
with `scope`, `id`, `operator`, and a type-matched `value`, plus `then` and `else`
action arrays. Bool and string conditions support `==` and `!=`; integers also
support `<`, `<=`, `>`, and `>=`. Scene definitions are stored in the scene's
top-level `variables` array; Global definitions are stored in
`.devilwood/global_variables.json` under the scenes directory.
Scene audio object actions use `PlayAudio`, `PauseAudio`, or `StopAudio` with an
`audio` object index. Scene object and hotspot actions use `ShowObject` or
`HideObject` with an `object` index, `DisableHotspot` with a `hotspot` index,
and `ChangeHotspotSetting` with a `hotspot` index plus updated `interaction`,
`check_variable`/`variable`, `require_item`/`item`/`consume_item`,
`disable_on_success`, and `fail_message` fields. Project-wide sound actions use `SoundOn`, `SoundOff`,
`SoundFadeIn`, or `SoundFadeOut` with a stable managed sound ID in `sound`;
fade actions also include a non-negative `duration` in seconds.
UI actions also support `EnableUI`, `DisableUI`, `FadeInUI`, and `FadeOutUI`;
fade actions target a group and include a non-negative `duration` in seconds.
Project UI elements have stable `id` values, optional boolean `visible` and
`enabled` initial states, and optional `alpha` (default 1). A root element
with `"group": true` is a UI group; `parent` uses the same object-index format
as scene hierarchies. `HotspotComp` stores an
`interaction` object index (or null) and requires Transform and box collider
components. Optionally add `item_interactions` entries (`{"item": -1,
"interaction": 0, "consume_item": false, "disable_on_success": false}`) to run
specific interactions when clicked with a selected inventory item, and/or enable
`check_variable` with `variable` (`scope` and `id`), `require_item` with `item`
and `consume_item`, `disable_on_success`, plus `fail_message` to run the
selected `interaction` only when the conditions pass (and otherwise display
`fail_message` as a subtitle). Clicking a slot in a
project UI `InventoryComp` selects or deselects its item (right-click also
deselects). Removing an object updates hotspot references.
An optional boolean `ui` marks an object and its descendants as UI; omitted
or false means scene layer unless an ancestor is UI.
An object's optional `parent` is the zero-based index of another object in the
scene's `objects` array. Transform positions are local to the parent; the
editor displays indented hierarchy entries and prevents cycles. Child sprites
inherit the parent's position, rotation, and scale. A parented object without
an explicit Transform uses an identity local Transform; collider bounds follow
the inherited position and scale. Removing a parent makes its direct children
roots while preserving the other links.
Older sprite objects remain readable and are converted to components when
edited; missing legacy scale defaults to `1, 1`.
Unsupported object data is preserved when saving, but runtime scenes only
accept supported components: Transform, Sprite, Panel_Sprite, RectangleComp,
colliders, TextComp, LightComp, AudioComp, HotspotComp, InteractionComp, and
InventoryItemComp.
`InventoryItemComp` stores an integer `item` ID from the project's data
inventory (`.devilwood/inventory_items.json`) and requires a `Sprite`
component; clicking the sprite at runtime adds that item to the runtime
inventory and hides the scene object. Use **Add inventory item** in the scene
panel to create one in a single click.
RectangleComp stores positive `width` and `height`, RGBA `color`, and independent
`opacity` from 0 (transparent) to 1 (opaque). Its optional integer `render_order`
defaults to `0`; lower values draw first and higher values draw on top. For example:
`"RectangleComp": {"width": 128, "height": 64, "color": {"r": 80, "g": 160, "b": 255, "a": 255}, "opacity": 0.75}`;
its size follows the object's Transform. Existing built-in C++ scenes remain
available alongside registered JSON scenes. Use
**Play** to run the configured startup scene, or the active scene when no
startup scene is selected.

Raylib can be built with `SUPPORT_CUSTOM_FRAME_CONTROL`, which requires the
game to manage screen swaps, input polling, and frame timing. The CMake option
`DEVILWOOD_CUSTOM_FRAME_CONTROL` must match the linked raylib build. It defaults
to ON for raylib 6 or later with the vcpkg toolchain, and OFF otherwise. If
your raylib build uses different settings, configure with
`-DDEVILWOOD_CUSTOM_FRAME_CONTROL=ON` or `OFF` as appropriate.
