#include "CraftingRecipes.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

std::filesystem::path CraftingRecipes::Path(const std::filesystem::path& sceneDirectory)
{
    return sceneDirectory / ".devilwood" / "crafting_recipes.json";
}

CraftingRecipes CraftingRecipes::Load(const std::filesystem::path& sceneDirectory)
{
    const auto path = Path(sceneDirectory);
    if (!std::filesystem::exists(path))
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open crafting recipe catalog.");
    return Parse(nlohmann::json::parse(input));
}

CraftingRecipes CraftingRecipes::Parse(const nlohmann::json& data)
{
    if (!data.is_object() || data.value("version", 0) != 1 ||
        !data.contains("recipes") || !data["recipes"].is_array())
        throw std::runtime_error("Crafting catalog must contain version 1 and a recipes array.");
    CraftingRecipes catalog;
    std::set<std::vector<int>> combinations;
    for (const auto& entry : data["recipes"]) {
        if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string() ||
            !entry.contains("ingredients") || !entry["ingredients"].is_array() ||
            !entry.contains("result") || !entry["result"].is_number_integer())
            throw std::runtime_error("Crafting recipe needs a name, ingredients and result.");
        CraftingRecipe recipe;
        recipe.name = entry["name"].get<std::string>();
        if (recipe.name.find_first_not_of(" \t\r\n") == std::string::npos ||
            entry["ingredients"].empty() || entry["ingredients"].size() > 3)
            throw std::runtime_error("Crafting recipes need a name and one to three ingredients.");
        for (const auto& ingredient : entry["ingredients"]) {
            if (!ingredient.is_number_integer() || ingredient.get<int>() >= 0)
                throw std::runtime_error("Crafting ingredients must be inventory item IDs.");
            recipe.ingredients.push_back(ingredient.get<int>());
        }
        recipe.result = entry["result"].get<int>();
        if (recipe.result >= 0)
            throw std::runtime_error("Crafting result must be an inventory item ID.");
        auto key = recipe.ingredients;
        std::sort(key.begin(), key.end());
        if (std::adjacent_find(key.begin(), key.end()) != key.end())
            throw std::runtime_error("A crafting recipe cannot use the same unique item twice.");
        if (!combinations.insert(key).second)
            throw std::runtime_error("Crafting recipes cannot have the same ingredients.");
        catalog.recipes.push_back(std::move(recipe));
    }
    return catalog;
}

int CraftingRecipes::Match(const std::array<int, 3>& slots) const
{
    std::vector<int> items;
    for (int id : slots)
        if (id < 0)
            items.push_back(id);
    std::sort(items.begin(), items.end());
    for (const auto& recipe : recipes) {
        auto ingredients = recipe.ingredients;
        std::sort(ingredients.begin(), ingredients.end());
        if (ingredients == items)
            return recipe.result;
    }
    return 0;
}

void CraftingRecipes::ValidateItems(const std::vector<int>& ids) const
{
    for (const auto& recipe : recipes) {
        for (const int id : recipe.ingredients)
            if (std::find(ids.begin(), ids.end(), id) == ids.end())
                throw std::runtime_error("Recipe '" + recipe.name + "' references a missing ingredient.");
        if (std::find(ids.begin(), ids.end(), recipe.result) == ids.end())
            throw std::runtime_error("Recipe '" + recipe.name + "' references a missing result.");
    }
}
