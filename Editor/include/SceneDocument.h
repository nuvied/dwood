#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class SceneDocument
{
public:
    static std::vector<std::filesystem::path> List(const std::filesystem::path& directory);
    static SceneDocument Create(const std::filesystem::path& directory, const std::string& id);
    static SceneDocument Open(const std::filesystem::path& path);

    const std::string& Name() const { return name; }
    const std::filesystem::path& Path() const { return path; }
    std::size_t ObjectCount() const { return data.at("objects").size(); }
    bool IsDirty() const { return !saved || name != savedName || data != savedData; }
    void SetName(const std::string& value) { name = value; }
    nlohmann::json Object(std::size_t index) const;
    void AddObject();
    void SetObject(std::size_t index, const nlohmann::json& object);
    std::optional<std::size_t> Parent(std::size_t index) const;
    void SetParent(std::size_t index, std::optional<std::size_t> parent);
    void RemoveObject(std::size_t index);
    void Save();

private:
    std::filesystem::path path;
    nlohmann::json data;
    nlohmann::json savedData;
    std::string name;
    std::string savedName;
    std::string savedContents;
    bool saved = false;
};
