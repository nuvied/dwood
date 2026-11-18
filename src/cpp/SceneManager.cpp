#include "SceneManager.h"

#include "SceneDocument.h"
#include "SceneObject.h"
#include "Screen.h"
#include "RenderSettings.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

namespace
{
class JsonSceneScreen final : public Screen
{
public:
    explicit JsonSceneScreen(std::vector<std::unique_ptr<Entity>> roots)
    {
        for (auto& root : roots)
            addEntity(std::move(root));
    }
};

std::string CatalogSceneName(const nlohmann::json& value)
{
    if (!value.is_string())
        throw std::runtime_error("Scene catalog entries must be filenames.");
    const auto name = value.get<std::string>();
    const std::filesystem::path path(name);
    if (name.empty() || path.filename().string() != name || path.extension() != ".json")
        throw std::runtime_error("Scene catalog entries must be JSON filenames.");
    return name;
}

void ValidateRuntimeComponents(const nlohmann::json& object)
{
    if (!object.is_object() || !object.contains("components") ||
        !object["components"].is_object())
        throw std::runtime_error("Runtime scene objects must contain a components object.");

    static const std::set<std::string> supported = {
        "TransformComp", "Sprite", "Panel_Sprite", "ColliderComp", "CircleCol", "TextComp",
        "LightComp", "HotspotComp", "InteractionComp"
    };
    for (auto it = object["components"].begin(); it != object["components"].end(); ++it)
        if (supported.find(it.key()) == supported.end())
            throw std::runtime_error("Unsupported runtime scene component: " + it.key());
}

EditorSceneGraph LoadRuntimeGraph(const std::filesystem::path& path)
{
    auto document = SceneDocument::Open(path);
    std::vector<nlohmann::json> objects;
    objects.reserve(document.ObjectCount());
    std::size_t lightCount = 0;
    for (std::size_t i = 0; i < document.ObjectCount(); ++i) {
        auto object = document.Object(i);
        ValidateRuntimeComponents(object);
        if (object["components"].contains("LightComp") &&
            ++lightCount > RenderSettings::MaxSceneLights) {
            throw std::runtime_error(
                "Runtime scenes support at most " +
                std::to_string(RenderSettings::MaxSceneLights) + " point lights.");
        }
        objects.push_back(std::move(object));
    }
    auto graph = LoadSceneGraph(objects, true);
    for (auto* object : graph.objects) {
        if (object) {
            if (const auto* text = object->getComponent<TextComp>();
                text && !text->FontName().empty()) {
                std::string error;
                if (!ResourcesLoader::GetCustomFont(text->FontName(), &error))
                    throw std::runtime_error(error);
            }
            if (const auto* sprite = object->getComponent<Sprite>();
                sprite && !SourceFits(*sprite))
                throw std::runtime_error("Runtime scene sprite is outside its atlas or atlas is unavailable.");
        }
    }
    return graph;
}
}

SceneManager::SceneManager(std::filesystem::path directory)
    : directory(std::move(directory))
{
}

std::filesystem::path SceneManager::CatalogPath() const
{
    return directory / ".devilwood" / "scene_catalog.json";
}

std::string SceneManager::SceneKey(const std::filesystem::path& scenePath) const
{
    const auto root = std::filesystem::weakly_canonical(directory);
    const auto path = std::filesystem::weakly_canonical(scenePath);
    if (path.parent_path() != root || path.extension() != ".json")
        throw std::runtime_error("Game scenes must be JSON files directly inside the scene directory.");
    return path.filename().string();
}

SceneManager::Catalog SceneManager::ReadCatalog() const
{
    Catalog catalog;
    const auto path = CatalogPath();
    if (!std::filesystem::exists(path))
        return catalog;

    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Cannot open scene catalog: " + path.string());
    nlohmann::json data;
    input >> data;
    if (!data.is_object() || !data.contains("version") ||
        !data["version"].is_number_integer() || data["version"] != 1 ||
        !data.contains("scenes") || !data["scenes"].is_array())
        throw std::runtime_error("Scene catalog must contain version 1 and a scenes array.");

    std::set<std::string> uniqueScenes;
    for (const auto& value : data["scenes"]) {
        const auto name = CatalogSceneName(value);
        if (!uniqueScenes.insert(name).second)
            throw std::runtime_error("Scene catalog contains a duplicate scene: " + name);
        catalog.scenes.push_back(name);
    }

    if (data.contains("startup") && !data["startup"].is_null()) {
        const auto name = CatalogSceneName(data["startup"]);
        if (uniqueScenes.find(name) == uniqueScenes.end())
            throw std::runtime_error("Startup scene must be registered in the scene catalog.");
        catalog.startup = name;
    }
    if (data.contains("startup_fade_in")) {
        if (!data["startup_fade_in"].is_boolean())
            throw std::runtime_error("Scene catalog startup_fade_in must be a boolean.");
        catalog.startupFadeIn = data["startup_fade_in"].get<bool>();
    }
    if (catalog.startupFadeIn && !catalog.startup)
        throw std::runtime_error("Startup fade-in requires a startup scene.");
    return catalog;
}

void SceneManager::WriteCatalog(const Catalog& catalog) const
{
    const auto path = CatalogPath();
    std::filesystem::create_directories(path.parent_path());
    nlohmann::json data = {
        {"version", 1},
        {"scenes", catalog.scenes},
        {"startup", catalog.startup ? nlohmann::json(*catalog.startup) : nlohmann::json(nullptr)},
        {"startup_fade_in", catalog.startupFadeIn}
    };
    std::ofstream output(path, std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot write scene catalog: " + path.string());
    output << data.dump(2) << '\n';
    output.flush();
    if (!output)
        throw std::runtime_error("Cannot finish writing scene catalog: " + path.string());
}

std::vector<std::filesystem::path> SceneManager::RegisteredScenes() const
{
    const auto catalog = ReadCatalog();
    std::vector<std::filesystem::path> result;
    result.reserve(catalog.scenes.size());
    for (const auto& name : catalog.scenes)
        result.push_back(directory / name);
    return result;
}

std::optional<std::filesystem::path> SceneManager::StartupScene() const
{
    const auto catalog = ReadCatalog();
    if (!catalog.startup)
        return std::nullopt;
    return directory / *catalog.startup;
}

bool SceneManager::StartupFadeIn() const
{
    return ReadCatalog().startupFadeIn;
}

void SceneManager::RegisterScene(const std::filesystem::path& scenePath)
{
    const auto name = SceneKey(scenePath);
    LoadRuntimeGraph(directory / name);
    auto catalog = ReadCatalog();
    if (std::find(catalog.scenes.begin(), catalog.scenes.end(), name) == catalog.scenes.end())
        catalog.scenes.push_back(name);
    WriteCatalog(catalog);
}

void SceneManager::UnregisterScene(const std::filesystem::path& scenePath)
{
    const auto name = SceneKey(scenePath);
    auto catalog = ReadCatalog();
    catalog.scenes.erase(std::remove(catalog.scenes.begin(), catalog.scenes.end(), name),
                         catalog.scenes.end());
    if (catalog.startup && *catalog.startup == name)
        catalog.startup.reset();
    if (!catalog.startup)
        catalog.startupFadeIn = false;
    WriteCatalog(catalog);
}

void SceneManager::SetStartupScene(const std::filesystem::path& scenePath)
{
    const auto name = SceneKey(scenePath);
    auto catalog = ReadCatalog();
    if (std::find(catalog.scenes.begin(), catalog.scenes.end(), name) == catalog.scenes.end())
        throw std::runtime_error("Add the scene to the game before setting it as the startup scene.");
    LoadRuntimeGraph(directory / name);
    catalog.startup = name;
    WriteCatalog(catalog);
}

void SceneManager::SetStartupFadeIn(bool enabled)
{
    auto catalog = ReadCatalog();
    if (!catalog.startup)
        throw std::runtime_error("Set a startup scene before configuring its fade-in transition.");
    catalog.startupFadeIn = enabled;
    WriteCatalog(catalog);
}

std::unique_ptr<Screen> SceneManager::LoadScene(const std::filesystem::path& scenePath) const
{
    const auto name = SceneKey(scenePath);
    const auto catalog = ReadCatalog();
    if (std::find(catalog.scenes.begin(), catalog.scenes.end(), name) == catalog.scenes.end())
        throw std::runtime_error("Scene is not registered in the game scene catalog.");

    auto graph = LoadRuntimeGraph(directory / name);
    return std::make_unique<JsonSceneScreen>(std::move(graph.roots));
}
