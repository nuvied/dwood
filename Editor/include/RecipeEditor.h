#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

class RecipeEditor
{
public:
    explicit RecipeEditor(std::filesystem::path sceneDirectory);
    void Draw(bool& open);
    bool SaveForRun();
private:
    bool Save();
    std::filesystem::path sceneDirectory;
    nlohmann::json document;
    std::optional<std::size_t> selected;
    std::string message;
    bool error = false;
    bool dirty = false;
    bool loaded = false;
};
