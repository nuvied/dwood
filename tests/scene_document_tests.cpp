#include "SceneDocument.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template<typename F>
void RequireFailure(F action, const char* message)
{
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void Write(const fs::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file)
        throw std::runtime_error("Unable to write test file");
}

int main()
{
    const auto directory = fs::temp_directory_path() /
        ("devilwood-scene-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto scenePath = directory / "forest.json";
    const auto invalidPath = directory / "invalid.json";
    const auto collisionPath = directory / "collision.json";
    const auto hierarchyPath = directory / "hierarchy.json";
    const auto interactionsPath = directory / "interactions.json";
    try {
        Require(SceneDocument::List(directory).empty(), "New directory should be empty");
        RequireFailure([&] { SceneDocument::Create(directory, "../outside"); }, "Unsafe ID accepted");
        RequireFailure([&] { SceneDocument::Create(directory, "CON"); }, "Reserved ID accepted");

        auto scene = SceneDocument::Create(directory, "forest");
        Require(scene.IsDirty(), "New scene must be unsaved");
        Require(scene.Type() == SceneType::Main && !scene.ParentScene(),
                "New scenes should default to the main type");
        Require(scene.BaseDarkness() == 0.65f, "New scene has the wrong base darkness");
        scene.SetBaseDarkness(0.35f);
        scene.Save();
        Require(!scene.IsDirty(), "Save did not clear dirty state");
        Require(SceneDocument::Open(scene.Path()).BaseDarkness() == 0.35f,
                "Scene darkness did not survive saving");
        auto popup = SceneDocument::Create(directory, "inventory_popup");
        popup.SetType(SceneType::Popup, scene.Path().filename().string());
        Require(popup.Undo() && popup.Type() == SceneType::Main,
                "Undo did not restore the prior scene type");
        Require(popup.Redo() && popup.Type() == SceneType::Popup &&
                popup.ParentScene() == std::optional<std::string>("forest.json"),
                "Redo did not restore popup scene metadata");
        popup.Save();
        const auto reopenedPopup = SceneDocument::Open(popup.Path());
        Require(reopenedPopup.Type() == SceneType::Popup &&
                reopenedPopup.ParentScene() == std::optional<std::string>("forest.json"),
                "Popup scene metadata did not survive saving");
        RequireFailure([&] { popup.SetType(SceneType::Popup, "../forest.json"); },
                       "Unsafe popup parent was accepted");
        RequireFailure([&] { popup.SetType(SceneType::Popup, "inventory_popup.json"); },
                       "Popup scene accepted itself as its parent");
        popup.SetType(SceneType::Main);
        Require(!popup.ParentScene(), "Changing a popup to main retained its parent");
        popup.Save();
        const std::vector<VariableDefinition> sceneVariables = {
            {"scene_flag", "door open", false},
            {"scene_score", "score", 10},
            {"scene_text", "status", std::string("ready")}
        };
        scene.SetVariables(sceneVariables);
        Require(scene.IsDirty(), "Changing scene variables did not dirty the scene");
        scene.Save();
        const auto reopenedScene = SceneDocument::Open(scene.Path());
        Require(reopenedScene.Variables().size() == sceneVariables.size() &&
                std::get<int>(reopenedScene.Variables()[1].defaultValue) == 10,
                "Scene variables did not survive saving");

        const std::vector<VariableDefinition> globalVariables = {
            {"global_flag", "has_key", false},
            {"global_count", "level", 2},
            {"global_text", "player name", std::string("Player")}
        };
        VariableStore::SaveGlobalDefinitions(directory, globalVariables);
        const auto loadedGlobals = VariableStore::LoadGlobalDefinitions(directory);
        Require(loadedGlobals.size() == globalVariables.size() &&
                std::get<std::string>(loadedGlobals[2].defaultValue) == "Player",
                "Global variable definitions did not round-trip");
        VariableStore runtimeVariables;
        runtimeVariables.ResetGlobals(loadedGlobals);
        runtimeVariables.ResetScene(sceneVariables);
        Require(runtimeVariables.Definitions(VariableScope::Global).size() == globalVariables.size() &&
                runtimeVariables.Definitions(VariableScope::Scene).size() == sceneVariables.size(),
                "Runtime variable definitions were not retained for inspection");
        runtimeVariables.Set(VariableScope::Global, "global_count", 3);
        runtimeVariables.Set(VariableScope::Scene, "scene_flag", true);
        Require(std::get<int>(runtimeVariables.Get(VariableScope::Global, "global_count")) == 3 &&
                std::get<bool>(runtimeVariables.Get(VariableScope::Scene, "scene_flag")),
                "Runtime variable writes did not preserve scope and type");
        runtimeVariables.ResetScene({});
        Require(runtimeVariables.Definitions(VariableScope::Scene).empty(),
                "Resetting runtime scene variables did not clear their definitions");
        runtimeVariables.ResetScene(sceneVariables);
        Require(VariableStore::Compare(3, ">=", 2),
                "Integer variable comparison failed");
        RequireFailure([&] {
            runtimeVariables.Set(VariableScope::Global, "global_count", std::string("wrong"));
        }, "Variable store accepted a value with the wrong type");
        auto variableHistory = SceneDocument::Create(directory, "variable_history");
        const std::vector<VariableDefinition> initialVariables = {
            {"history_count", "count", 1}
        };
        variableHistory.SetVariables(initialVariables);
        auto editedVariables = initialVariables;
        editedVariables[0].defaultValue = 2;
        variableHistory.SetVariables(editedVariables, true);
        editedVariables[0].defaultValue = 3;
        variableHistory.SetVariables(editedVariables, true);
        variableHistory.EndUndoGroup();
        Require(variableHistory.Undo() &&
                std::get<int>(variableHistory.Variables()[0].defaultValue) == 1,
                "Undo did not restore scene variable definitions");
        Require(variableHistory.Redo() &&
                std::get<int>(variableHistory.Variables()[0].defaultValue) == 3,
                "Redo did not restore scene variable definitions");
        RequireFailure([&] { scene.SetBaseDarkness(1.1f); },
                       "Out-of-range scene darkness was accepted");
        Require(SceneDocument::List(directory).size() == 2, "Saved scenes were not listed");
        RequireFailure([&] { SceneDocument::Create(directory, "forest"); }, "Existing scene accepted");

        auto undoScene = SceneDocument::Create(directory, "undo");
        Require(!undoScene.CanUndo() && !undoScene.CanRedo(),
                "New scenes should start with an empty undo history");
        const nlohmann::json door = {
            {"name", "Door"}, {"components", nlohmann::json::object()}
        };
        undoScene.AddObject(door);
        Require(undoScene.ObjectCount() == 1 && undoScene.CanUndo(),
                "Adding an object did not create an undo step");
        Require(undoScene.Undo() && undoScene.ObjectCount() == 0,
                "Undo did not remove the added object");
        Require(undoScene.Redo() && undoScene.Object(0) == door,
                "Redo did not restore the added object");
        auto gate = undoScene.Object(0);
        gate["name"] = "Gate";
        undoScene.SetObject(0, gate);
        Require(undoScene.Undo() && undoScene.Object(0) == door,
                "Undo did not restore the prior object contents");
        Require(undoScene.Redo() && undoScene.Object(0) == gate,
                "Redo did not restore the edited object contents");
        undoScene.SetName("Undo scene", true);
        undoScene.SetName("Undo scene renamed", true);
        undoScene.EndUndoGroup();
        Require(undoScene.Undo() && undoScene.Name() == "undo",
                "Undo did not revert a coalesced scene-name edit");
        Require(undoScene.Redo() && undoScene.Name() == "Undo scene renamed",
                "Redo did not restore a coalesced scene-name edit");
        undoScene.SetBaseDarkness(0.4f);
        Require(undoScene.Undo() && undoScene.BaseDarkness() == 0.65f,
                "Undo did not restore the prior scene darkness");
        Require(undoScene.Redo() && undoScene.BaseDarkness() == 0.4f,
                "Redo did not restore the edited scene darkness");
        undoScene.RemoveObject(0);
        Require(undoScene.Undo() && undoScene.Object(0) == gate,
                "Undo did not restore a removed object");
        Require(undoScene.Undo(), "Undo should return to the state before the darkness edit");
        undoScene.SetBaseDarkness(0.5f);
        Require(!undoScene.CanRedo(), "A new edit after undo should clear the redo history");

        auto collision = SceneDocument::Create(directory, "collision");
        Write(collisionPath, R"({"version":1,"name":"Other","objects":[]})");
        RequireFailure([&] { collision.Save(); }, "New scene overwrote an existing file");
        Require(SceneDocument::Open(collisionPath).Name() == "Other", "Existing file was not preserved");
        fs::remove(collisionPath);

        auto hierarchy = SceneDocument::Create(directory, "hierarchy");
        hierarchy.AddObject();
        hierarchy.AddObject();
        hierarchy.AddObject();
        hierarchy.SetParent(1, 0);
        Require(hierarchy.Undo() && !hierarchy.Parent(1),
                "Undo did not remove a parent relationship");
        Require(hierarchy.Redo() && hierarchy.Parent(1) == 0,
                "Redo did not restore a parent relationship");
        hierarchy.SetParent(2, 1);
        Require(hierarchy.Parent(1) == 0 && hierarchy.Parent(2) == 1,
                "Parent relationships were not stored");
        RequireFailure([&] { hierarchy.SetParent(0, 2); }, "Hierarchy cycle accepted");
        RequireFailure([&] { hierarchy.SetParent(1, 1); }, "Object accepted itself as parent");
        hierarchy.Save();
        auto hierarchyReloaded = SceneDocument::Open(hierarchyPath);
        Require(hierarchyReloaded.Parent(2) == 1, "Parent relationship did not round-trip");
        hierarchyReloaded.RemoveObject(0);
        Require(!hierarchyReloaded.Parent(0) && hierarchyReloaded.Parent(1) == 0,
                "Deleting a parent did not safely rebase child references");
        Require(hierarchyReloaded.Undo() && hierarchyReloaded.ObjectCount() == 3 &&
                hierarchyReloaded.Parent(1) == 0 && hierarchyReloaded.Parent(2) == 1,
                "Undo did not restore a deleted parent and its hierarchy");

        auto interactions = SceneDocument::Create(directory, "interactions");
        interactions.AddObject();
        interactions.AddObject();
        interactions.AddObject();
        auto hotspot = interactions.Object(0);
        hotspot["components"]["HotspotComp"] = {{"interaction", 2}};
        hotspot["components"]["InteractionComp"] = {
            {"actions", {{{"type", "PlayAudio"}, {"audio", 2}}}}
        };
        interactions.SetObject(0, hotspot);
        interactions.RemoveObject(1);
        Require(interactions.Object(0)["components"]["HotspotComp"]["interaction"] == 1 &&
                interactions.Object(0)["components"]["InteractionComp"]["actions"][0]["audio"] == 1,
                "Deleting another object did not rebase object references");
        interactions.Save();
        auto reloadedInteractions = SceneDocument::Open(interactions.Path());
        Require(reloadedInteractions.Object(0)["components"]["HotspotComp"]["interaction"] == 1 &&
                reloadedInteractions.Object(0)["components"]["InteractionComp"]["actions"][0]["audio"] == 1,
                "Object references did not survive saving");
        reloadedInteractions.RemoveObject(1);
        Require(reloadedInteractions.Object(0)["components"]["HotspotComp"]["interaction"].is_null() &&
                reloadedInteractions.Object(0)["components"]["InteractionComp"]["actions"][0]["audio"].is_null(),
                "Deleting a referenced object did not clear its references");

        auto reopened = SceneDocument::Open(scenePath);
        Require(reopened.Name() == "forest" && reopened.ObjectCount() == 0, "Scene did not round-trip");
        reopened.SetName("Forest entrance");
        Require(reopened.IsDirty(), "Renaming did not mark scene dirty");
        reopened.Save();
        Require(SceneDocument::Open(scenePath).Name() == "Forest entrance", "Name was not saved");
        reopened.SetName("Forest exit");
        Require(reopened.Undo() && reopened.Name() == "Forest entrance" && !reopened.IsDirty(),
                "Undo after saving did not return to the saved scene state");
        Require(reopened.Redo() && reopened.Name() == "Forest exit" && reopened.IsDirty(),
                "Redo after saving did not restore the unsaved scene state");
        Require(reopened.Undo(), "Undo should restore the saved scene name");

        reopened.AddObject();
        Require(reopened.IsDirty() && reopened.ObjectCount() == 1, "Adding an object did not mark scene dirty");
        auto object = reopened.Object(0);
        Require(object["name"] == "Object" && object["components"].empty(), "New object is not empty");
        object["name"] = "Door";
        object["components"]["TransformComp"] = {
            {"position", {{"x", -10}, {"y", 23}}}, {"scale", {{"x", 0.5}, {"y", 2}}}};
        object["components"]["Sprite"] = {
            {"atlas", "ui_page.png"}, {"source", {{"x", 12}, {"y", 34}, {"width", 56}, {"height", 78}}}};
        object["components"]["Future"] = {{"value", 7}};
        object["locked"] = true;
        reopened.SetObject(0, object);
        auto invalidObject = object;
        invalidObject["name"] = " ";
        RequireFailure([&] { reopened.SetObject(0, invalidObject); }, "Blank object name accepted");
        reopened.Save();
        auto objectScene = SceneDocument::Open(scenePath);
        Require(objectScene.Object(0) == object, "Object components did not round-trip");
        objectScene.SetObject(0, object);
        Require(!objectScene.IsDirty(), "Unchanged object marked scene dirty");
        object["components"].erase("Sprite");
        objectScene.SetObject(0, object);
        Require(objectScene.IsDirty(), "Removing a component did not mark scene dirty");
        objectScene.Save();
        Require(SceneDocument::Open(scenePath).Object(0) == object, "Removing a component was not saved");
        Require(SceneDocument::Open(scenePath).Object(0).value("locked", false),
                "Locked scene object state did not survive saving");
        objectScene.RemoveObject(0);
        Require(objectScene.IsDirty() && objectScene.ObjectCount() == 0, "Removing an object did not mark scene dirty");
        objectScene.Save();
        Require(SceneDocument::Open(scenePath).ObjectCount() == 0, "Removed object was not saved");

        Write(scenePath, R"({"version":1,"name":"Forest","objects":[{"id":"item"}],"future":{"value":7}})");
        auto extended = SceneDocument::Open(scenePath);
        Require(extended.Object(0)["id"] == "item" &&
                extended.Type() == SceneType::Main && !extended.ParentScene(),
                "Legacy scene did not default to Main or preserve unsupported object data");
        extended.SetName("Updated");
        extended.Save();
        std::ifstream input(scenePath);
        auto json = nlohmann::json::parse(input);
        input.close();
        Require(json["objects"][0]["id"] == "item" && json["future"]["value"] == 7,
                "Saving discarded uneditable scene data");

        Write(scenePath, R"({"version":1,"name":"Forest","objects":[{"type":"sprite","name":"Door","atlas":"atlas_1.png","position":{"x":1,"y":2,"extra":3},"source":{"x":0,"y":0,"width":5,"height":6,"extra":4},"future":9},{"type":"sprite","name":"Bad","atlas":"atlas_1.png","position":{"x":18446744073709551615,"y":0},"source":{"x":0,"y":0,"width":1,"height":1}}]})");
        auto withExtras = SceneDocument::Open(scenePath);
        auto editable = withExtras.Object(0);
        Require(!withExtras.IsDirty() && editable["components"]["TransformComp"]["scale"]["x"] == 1,
                "Legacy sprite view should default to unit scale without changing the document");
        editable["components"]["TransformComp"]["position"]["x"] = 5;
        withExtras.SetObject(0, editable);
        withExtras.Save();
        std::ifstream extrasInput(scenePath);
        const auto extrasJson = nlohmann::json::parse(extrasInput);
        extrasInput.close();
        Require(extrasJson["objects"][0]["future"] == 9 &&
                extrasJson["objects"][0]["components"]["TransformComp"]["position"]["extra"] == 3 &&
                extrasJson["objects"][0]["components"]["Sprite"]["source"]["extra"] == 4 &&
                extrasJson["objects"][1]["position"]["x"] == 18446744073709551615ULL,
                "Editing a sprite discarded unknown data");

        auto stale = SceneDocument::Open(scenePath);
        stale.SetName("Stale");
        Write(scenePath, R"({"version":1,"name":"External","objects":[]})");
        RequireFailure([&] { stale.Save(); }, "External changes were overwritten");
        Require(SceneDocument::Open(scenePath).Name() == "External", "External edit was not preserved");

        Write(invalidPath, R"({"version":2,"name":"Future","objects":[]})");
        RequireFailure([&] { SceneDocument::Open(invalidPath); }, "Unsupported version accepted");
        Write(invalidPath, R"({"version":1,"name":"Darkness","objects":[],"settings":{"darkness":-0.1}})");
        RequireFailure([&] { SceneDocument::Open(invalidPath); },
                       "Out-of-range scene darkness was accepted when loading");
        Write(invalidPath, "{broken");
        RequireFailure([&] { SceneDocument::Open(invalidPath); }, "Invalid JSON accepted");
        extended.SetName("  ");
        RequireFailure([&] { extended.Save(); }, "Blank name accepted");

        fs::remove_all(directory);
        std::cout << "Scene document tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::error_code ec;
        fs::remove_all(directory, ec);
        return 1;
    }
}
