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
        scene.Save();
        Require(!scene.IsDirty(), "Save did not clear dirty state");
        Require(SceneDocument::List(directory).size() == 1, "Saved scene was not listed");
        RequireFailure([&] { SceneDocument::Create(directory, "forest"); }, "Existing scene accepted");

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

        auto interactions = SceneDocument::Create(directory, "interactions");
        interactions.AddObject();
        interactions.AddObject();
        interactions.AddObject();
        auto hotspot = interactions.Object(0);
        hotspot["components"]["HotspotComp"] = {{"interaction", 2}};
        interactions.SetObject(0, hotspot);
        interactions.RemoveObject(1);
        Require(interactions.Object(0)["components"]["HotspotComp"]["interaction"] == 1,
                "Deleting another object did not rebase hotspot interaction");
        interactions.Save();
        auto reloadedInteractions = SceneDocument::Open(interactions.Path());
        Require(reloadedInteractions.Object(0)["components"]["HotspotComp"]["interaction"] == 1,
                "Hotspot interaction did not survive saving");
        reloadedInteractions.RemoveObject(1);
        Require(reloadedInteractions.Object(0)["components"]["HotspotComp"]["interaction"].is_null(),
                "Deleting interaction did not clear hotspot reference");

        auto reopened = SceneDocument::Open(scenePath);
        Require(reopened.Name() == "forest" && reopened.ObjectCount() == 0, "Scene did not round-trip");
        reopened.SetName("Forest entrance");
        Require(reopened.IsDirty(), "Renaming did not mark scene dirty");
        reopened.Save();
        Require(SceneDocument::Open(scenePath).Name() == "Forest entrance", "Name was not saved");

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
        Require(extended.Object(0)["id"] == "item", "Unsupported object was not preserved");
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
        Write(invalidPath, "{broken");
        RequireFailure([&] { SceneDocument::Open(invalidPath); }, "Invalid JSON accepted");
        extended.SetName("  ");
        RequireFailure([&] { extended.Save(); }, "Blank name accepted");

        fs::remove(scenePath);
        fs::remove(invalidPath);
        fs::remove(hierarchyPath);
        fs::remove(interactionsPath);
        fs::remove(directory);
        std::cout << "Scene document tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::error_code ec;
        fs::remove(scenePath, ec);
        fs::remove(invalidPath, ec);
        fs::remove(collisionPath, ec);
        fs::remove(hierarchyPath, ec);
        fs::remove(interactionsPath, ec);
        fs::remove(directory, ec);
        return 1;
    }
}
