#include "SceneDocument.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace
{
std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open scene file: " + path.string());
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad())
        throw std::runtime_error("Cannot read scene file: " + path.string());
    return contents;
}

void ValidateId(const std::string& id)
{
    if (id.empty() || id.size() > 64 ||
        !std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        }))
        throw std::runtime_error("Scene ID must contain 1-64 letters, numbers, underscores, or hyphens.");

    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
    });
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
        (upper.size() == 4 && (upper.substr(0, 3) == "COM" || upper.substr(0, 3) == "LPT") &&
         upper[3] >= '1' && upper[3] <= '9'))
        throw std::runtime_error("Scene ID is a reserved filename.");
}

void ValidateName(const std::string& name)
{
    if (name.empty() || name.size() > 127 ||
        name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Scene name must contain 1-127 characters and cannot be blank.");
}

void ValidateHierarchy(const nlohmann::json& objects)
{
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto& object = objects[i];
        if (!object.is_object() || !object.contains("parent"))
            continue;
        const auto& value = object["parent"];
        if (!value.is_number_integer())
            throw std::runtime_error("Object parent must be an object index.");
        std::uint64_t parent = 0;
        if (value.is_number_unsigned()) {
            parent = value.get<std::uint64_t>();
        } else {
            const auto signedParent = value.get<std::int64_t>();
            if (signedParent < 0)
                throw std::runtime_error("Object parent index cannot be negative.");
            parent = static_cast<std::uint64_t>(signedParent);
        }
        if (parent >= objects.size() || parent == i)
            throw std::runtime_error("Object parent index is invalid.");
    }

    for (std::size_t i = 0; i < objects.size(); ++i) {
        std::vector<bool> visited(objects.size(), false);
        std::size_t current = i;
        while (objects[current].is_object() && objects[current].contains("parent")) {
            if (visited[current])
                throw std::runtime_error("Scene hierarchy cannot contain a cycle.");
            visited[current] = true;
            current = objects[current]["parent"].get<std::size_t>();
        }
    }
}

nlohmann::json ObjectForEditing(const nlohmann::json& object)
{
    if (!object.is_object() || object.contains("components") ||
        object.value("type", nlohmann::json()) != "sprite" ||
        !object.contains("name") || !object["name"].is_string() ||
        !object.contains("atlas") || !object["atlas"].is_string() ||
        !object.contains("position") || !object["position"].is_object() ||
        !object.contains("source") || !object["source"].is_object() ||
        (object.contains("scale") && !object["scale"].is_object()))
        return object;

    auto result = object;
    auto transform = nlohmann::json::object();
    transform["position"] = result["position"];
    transform["scale"] = result.value("scale", nlohmann::json{{"x", 1.0}, {"y", 1.0}});
    auto sprite = nlohmann::json{{"atlas", result["atlas"]}, {"source", result["source"]}};
    result["components"] = {{"TransformComp", std::move(transform)}, {"Sprite", std::move(sprite)}};
    result.erase("type");
    result.erase("position");
    result.erase("scale");
    result.erase("atlas");
    result.erase("source");
    return result;
}
}

std::vector<std::filesystem::path> SceneDocument::List(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(directory))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            result.push_back(entry.path());
    std::sort(result.begin(), result.end());
    return result;
}

SceneDocument SceneDocument::Create(const std::filesystem::path& directory, const std::string& id)
{
    ValidateId(id);
    SceneDocument scene;
    scene.path = directory / (id + ".json");
    if (std::filesystem::exists(scene.path))
        throw std::runtime_error("A scene with that ID already exists.");
    scene.name = id;
    scene.data = {{"version", 1}, {"name", id}, {"objects", nlohmann::json::array()}};
    return scene;
}

SceneDocument SceneDocument::Open(const std::filesystem::path& path)
{
    SceneDocument scene;
    scene.path = path;
    scene.savedContents = ReadFile(path);
    try {
        scene.data = nlohmann::json::parse(scene.savedContents);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Invalid JSON in " + path.string() + ": " + e.what());
    }
    if (!scene.data.is_object() || !scene.data.contains("version") ||
        !scene.data["version"].is_number_integer() || scene.data["version"] != 1 ||
        !scene.data.contains("name") || !scene.data["name"].is_string() ||
        !scene.data.contains("objects") || !scene.data["objects"].is_array())
        throw std::runtime_error("Scene must contain version 1, a name, and an objects array.");
    ValidateHierarchy(scene.data["objects"]);
    scene.name = scene.data["name"].get<std::string>();
    ValidateName(scene.name);
    scene.savedName = scene.name;
    scene.savedData = scene.data;
    scene.saved = true;
    return scene;
}

nlohmann::json SceneDocument::Object(std::size_t index) const
{
    return ObjectForEditing(data.at("objects").at(index));
}

void SceneDocument::AddObject()
{
    data["objects"].push_back({{"name", "Object"}, {"components", nlohmann::json::object()}});
}

void SceneDocument::SetObject(std::size_t index, const nlohmann::json& object)
{
    if (!object.is_object() || !object.contains("name") || !object["name"].is_string() ||
        !object.contains("components") || !object["components"].is_object())
        throw std::runtime_error("Object must have a name and a components object.");
    ValidateName(object["name"].get<std::string>());
    auto objects = data.at("objects");
    objects.at(index) = object;
    ValidateHierarchy(objects);
    data.at("objects").at(index) = object;
}

std::optional<std::size_t> SceneDocument::Parent(std::size_t index) const
{
    const auto& object = data.at("objects").at(index);
    if (!object.is_object() || !object.contains("parent"))
        return std::nullopt;
    return object["parent"].get<std::size_t>();
}

void SceneDocument::SetParent(std::size_t index, std::optional<std::size_t> parent)
{
    auto objects = data.at("objects");
    auto& object = objects.at(index);
    if (!object.is_object())
        throw std::runtime_error("Only scene objects can be parented.");
    if (parent)
        object["parent"] = *parent;
    else
        object.erase("parent");
    ValidateHierarchy(objects);
    data["objects"] = std::move(objects);
}

void SceneDocument::RemoveObject(std::size_t index)
{
    auto& objects = data.at("objects");
    if (index >= objects.size())
        throw std::out_of_range("Object index is out of range.");
    objects.erase(index);
    for (auto& object : objects) {
        if (!object.is_object())
            continue;
        if (object.contains("parent")) {
            const auto parent = object["parent"].get<std::size_t>();
            if (parent == index)
                object.erase("parent");
            else if (parent > index)
                object["parent"] = parent - 1;
        }
        if (object.contains("components") && object["components"].is_object() &&
            object["components"].contains("HotspotComp")) {
            auto& hotspot = object["components"]["HotspotComp"];
            if (hotspot.is_object() && hotspot.contains("interaction") &&
                hotspot["interaction"].is_number_unsigned()) {
                const auto target = hotspot["interaction"].get<std::size_t>();
                if (target == index)
                    hotspot["interaction"] = nullptr;
                else if (target > index)
                    hotspot["interaction"] = target - 1;
            } else if (hotspot.is_object() && hotspot.contains("interaction") &&
                       hotspot["interaction"].is_number_integer() &&
                       hotspot["interaction"].get<std::int64_t>() >= 0) {
                const auto target = hotspot["interaction"].get<std::size_t>();
                if (target == index)
                    hotspot["interaction"] = nullptr;
                else if (target > index)
                    hotspot["interaction"] = target - 1;
            }
        }
    }
}

void SceneDocument::Save()
{
    ValidateName(name);
    if (saved) {
        if (ReadFile(path) != savedContents)
            throw std::runtime_error("Scene was changed outside the editor. Reopen it before saving.");
    } else if (std::filesystem::exists(path)) {
        throw std::runtime_error("A scene with that ID already exists.");
    }

    auto updated = data;
    updated["name"] = name;
    const std::string contents = updated.dump(2) + "\n";
    std::filesystem::create_directories(path.parent_path());
    auto tempDirectory = path;
    tempDirectory += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    if (!std::filesystem::create_directory(tempDirectory))
        throw std::runtime_error("Temporary scene directory already exists: " + tempDirectory.string());
    const auto temp = tempDirectory / path.filename();
    try {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot create temporary scene file: " + temp.string());
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output)
            throw std::runtime_error("Cannot write scene file: " + temp.string());
        if (saved ? ReadFile(path) != savedContents : std::filesystem::exists(path))
            throw std::runtime_error("Scene changed before it could be saved.");
        std::filesystem::rename(temp, path);
        data = std::move(updated);
        savedData = data;
        savedContents = contents;
        savedName = name;
        saved = true;
        std::filesystem::remove(tempDirectory);
    } catch (const std::exception& e) {
        std::error_code fileError, directoryError;
        std::filesystem::remove(temp, fileError);
        std::filesystem::remove(tempDirectory, directoryError);
        if (fileError || directoryError)
            throw std::runtime_error(std::string(e.what()) + "; cannot clean temporary scene directory: " +
                                     (fileError ? fileError.message() : directoryError.message()));
        throw;
    }
}
