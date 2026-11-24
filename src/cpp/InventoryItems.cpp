#include "Global.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

void ItemDB::Load(const std::filesystem::path& sceneDirectory)
{
    items_database.clear();
    const auto path = sceneDirectory / ".devilwood" / "inventory_items.json";
    if (!std::filesystem::exists(path))
        return;

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open inventory item catalog.");
        const auto data = nlohmann::json::parse(input);
        if (!data.is_object() || data.value("version", 0) != 1 ||
            !data.contains("items") || !data["items"].is_array())
            throw std::runtime_error("Inventory catalog must contain version 1 and an items array.");

        std::vector<Item> loaded;
        for (const auto& entry : data["items"]) {
            if (!entry.is_object() || !entry["id"].is_number_integer() ||
                !entry["name"].is_string() || !entry["description"].is_string() ||
                !entry["icon"].is_object() || !entry["icon"]["source"].is_object())
                throw std::runtime_error("Inventory catalog contains an invalid item.");
            const auto& source = entry["icon"]["source"];
            if (!source["x"].is_number() || !source["y"].is_number() ||
                !source["width"].is_number() || !source["height"].is_number())
                throw std::runtime_error("Inventory icon source must contain a rectangle.");
            if (entry.contains("carry_on_start") && !entry["carry_on_start"].is_boolean())
                throw std::runtime_error("Inventory carry-on-start setting must be a boolean.");
            const int id = entry["id"].get<int>();
            const auto name = entry["name"].get<std::string>();
            if (name.empty() || name.find_first_not_of(" \t\r\n") == std::string::npos)
                throw std::runtime_error("Inventory item name cannot be blank.");
            int recipeTag = static_cast<int>(NONE);
            if (entry.contains("recipe_tag")) {
                if (!entry["recipe_tag"].is_number_integer())
                    throw std::runtime_error("Inventory recipe tag must be an integer.");
                recipeTag = entry["recipe_tag"].get<int>();
            }
            if (recipeTag < static_cast<int>(EMPTY) || recipeTag > static_cast<int>(NONE))
                throw std::runtime_error("Inventory recipe tag is outside the supported range.");
            Item item(name, id, {
                source["x"].get<float>(), source["y"].get<float>(),
                source["width"].get<float>(), source["height"].get<float>()
            }, static_cast<RECIPE_TAG>(recipeTag));
            item.description = entry["description"].get<std::string>();
            item.carryOnStart = entry.value("carry_on_start", false);
            loaded.push_back(std::move(item));
        }
        items_database = std::move(loaded);
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Unable to load inventory items: %s", e.what());
    }
}
