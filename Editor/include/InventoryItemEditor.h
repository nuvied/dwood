#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class InventoryItemEditor
{
public:
    explicit InventoryItemEditor(std::filesystem::path sceneDirectory);

    void Draw(bool& open);
    bool SaveForRun();
    bool IsDirty() const { return dirty; }

private:
    struct IconFrame {
        std::string name;
        float x = 0;
        float y = 0;
        float width = 0;
        float height = 0;
    };

    void Load();
    bool Save();
    void AddItem();
    void RemoveItem(std::size_t index);
    void RefreshFrames();

    std::filesystem::path path;
    std::optional<nlohmann::json> document;
    std::optional<std::size_t> selected;
    std::vector<IconFrame> frames;
    std::string message;
    bool messageIsError = false;
    bool dirty = false;
};
