# Devilwood

The `devilwood` and `editor` targets expect `resource` and `shaders` in their
working directory. CMake copies changed assets to the build directory when
building the editor or game, so run both programs from that directory. If a
changed asset is in use, close the game or editor before rebuilding. The editor uses
Dear ImGui 1.92 or newer, imgui-node-editor, and nlohmann-json (available as
`imgui`, `imgui-node-editor`, and `nlohmann-json` in vcpkg). The node editor
is linked only to the editor target; vcpkg's imgui-node-editor 0.9.3#4 has
been tested.

The editor opens in the Scenes workspace. Enter a scene ID using letters,
numbers, `_` or `-`, select **New scene**, edit its display name, and select
**Save**. Select a JSON file in the list to reopen it. Files are saved under
the source project's `scenes` directory so they can be shared with the team;
pass a directory as the editor's first argument to use another location.
Unsaved changes are confirmed before switching scenes or closing the editor.
Use **Play** to run the game in the editor and **Stop** to return to scene
editing. **Build & Run** builds the runtime and opens it in a separate window.
Both actions save the open scene first; a save error prevents the run.
They still launch the configured startup scene, which can differ from the open scene.
Both run buttons change to Stop controls while their game is running. Use the
**Window** menu to show or hide editor panels.

Use **Add object**, select it, then use **Add component** to add Transform,
Sprite, panel sprite, box or circle collider, and text. Transform controls
position, rotation, and scale (positive X and Y multipliers; 1 means original
size). Sprite selects the atlas and its rectangle in pixels. Panel sprites use
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
from the scene view. Open **Window > Scene Settings** to set the project-wide
render width and height (default 1024x512), then select **Apply and save** to
update the editor and runtime render resolution. The configured dimensions are
the Game View/output size; the game renders internally at twice that resolution
and uses point filtering when displayed for a crisp result. Settings are stored in
`scenes/.devilwood/game_settings.json` (or in the selected scene directory).
Open **Window > Scene Manager** to add the saved current scene to the runtime
game catalog and load it for Play in Editor. Registered scenes are stored in
`scenes/.devilwood/scene_catalog.json`; **Set startup** selects the JSON scene
loaded by the game on launch. Enable **Fade in at startup** to fade into that
scene when the game starts or when Play is pressed. The in-editor **Game View**
fits the configured render target to its resizable window. Game code can
transition to a registered JSON scene with `Game::ChangeSceneF(scenePath)`.
Numeric fields use draggable controls: drag a value horizontally to adjust it,
or enter a value directly for precise edits.
Game View runs game `Entity` and component instances and uses their
drawing behavior, with collider outlines shown for editing. A Sprite without
Transform uses the game's default position at the origin. The atlas rectangle
must fit the chosen image. The atlas picker lists the four PNG resources
currently loaded by the game. Components and objects can be removed in the
inspector. Atlas rectangle selection by drawing a box is not implemented yet.
Use **Add light** in the scene panel to create a point-light object, then adjust
its scene-unit radius, intensity, and color in the inspector. The radial light
shader supports up to seven scene lights per scene, in addition to the gameplay
light slot; the scene view shows each light's radius gizmo. **Preview lighting**
shows the runtime light effect in the scene view and can be turned off.

Use **Add UI** to create a UI text object, or enable **UI layer** on an object.
UI descendants inherit the UI layer. UI objects draw over the lit scene using
the UI camera, so scene lights do not affect them. Add Sprite, panel sprite, or
Text components to build the UI.

Open **Window > UI Manager** and choose **Create UI Manager** to add an
optional project-wide UI shared across scenes. Add text, sprites, or panels,
select an element, and edit its properties or drag it in the game-sized preview.
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
editor** to view its actions as a linear graph. Add **Change Scene**, select
its node, and choose a scene registered in Scene Manager. Drag nodes to arrange
them, pan with the right mouse button, and zoom with the mouse wheel; **Fit
graph** resets the view. Links follow the action order automatically; branching
and manual connections are not supported. Node positions reset when closing
the node editor or selecting another interaction. The action inspector remains
available as well.
Use **Add UI action** to show, hide, enable, or disable a project UI element or
group, or to fade a group in or out over a chosen duration. Fade-in shows the
group and restores its configured alpha; fade-out hides it when finished.
Fading does not change enabled state, and later actions wait for the fade.
The option is disabled until a UI manager exists. Add UI actions before Change
Scene, which ends the current interaction. Deleting a UI element leaves any
existing references unresolved so they can be corrected in the editor.
Use **Add hotspot** to create a
clickable rectangle, adjust its Transform and box collider, and select the
interaction in its inspector. The rectangle is visible only in the editor.
Clicking the hotspot runs the interaction; clicks while it is running are
ignored. A scene change uses the existing fade transition and ends the current
scene's interaction. Register the target scene before triggering the action.

Scene files use `{"version": 1, "name": "...", "objects": []}`. A sprite
object has a `name` and a `components` object. `TransformComp` stores
`position`, `scale`, and optionally `rotation`; `Sprite` stores an `atlas`
PNG filename and a `source` rectangle with `x`, `y`, `width`, and `height`.
`Panel_Sprite` stores `margin`, `width`, and `height` and requires a `Sprite`.
`ColliderComp` stores `width` and `height` (sprite-backed colliders follow the
Sprite or panel size); `CircleCol` stores `radius` and optional `x` and `y`.
`TextComp` stores `text`, `size`, optional RGBA `color` channels (0-255), and
optional `margin` offset. Optional `font` is a `.ttf` or `.otf` filename in
`resource/fonts`; omit it or use an empty string for Raylib's default font.
`InteractionComp` stores an `actions` array; Change Scene entries use
`{"type":"ChangeScene","scene":"scene-id.json"}` and UI visibility entries use
`{"type":"ShowUI","ui":"element-id"}` or `{"type":"HideUI","ui":"element-id"}`.
UI actions also support `EnableUI`, `DisableUI`, `FadeInUI`, and `FadeOutUI`;
fade actions target a group and include a non-negative `duration` in seconds.
Project UI elements have stable `id` values, optional boolean `visible` and
`enabled` initial states, and optional `alpha` (default 1). A root element
with `"group": true` is a UI group; `parent` uses the same object-index format
as scene hierarchies. `HotspotComp` stores an
`interaction` object index (or null) and requires Transform and box collider
components. Removing an object updates hotspot references.
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
accept Transform, Sprite, panel sprite, collider, and text components. Existing
built-in C++ scenes remain available alongside registered JSON scenes. Use
**Play** to run the configured startup scene, or the active scene when no
startup scene is selected.

Raylib can be built with `SUPPORT_CUSTOM_FRAME_CONTROL`, which requires the
game to manage screen swaps, input polling, and frame timing. The CMake option
`DEVILWOOD_CUSTOM_FRAME_CONTROL` must match the linked raylib build. It defaults
to ON for raylib 6 or later with the vcpkg toolchain, and OFF otherwise. If
your raylib build uses different settings, configure with
`-DDEVILWOOD_CUSTOM_FRAME_CONTROL=ON` or `OFF` as appropriate.
