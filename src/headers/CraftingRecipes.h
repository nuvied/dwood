#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct CraftingRecipe
{
    std::string name;
    std::vector<int> ingredients;
    int result = 0;
};

class CraftingRecipes
{
public:
    static std::filesystem::path Path(const std::filesystem::path& sceneDirectory);
    static CraftingRecipes Load(const std::filesystem::path& sceneDirectory);
    static CraftingRecipes Parse(const nlohmann::json& data);
    int Match(const std::array<int, 3>& slots) const;
    void ValidateItems(const std::vector<int>& ids) const;
    std::vector<CraftingRecipe> recipes;
};
