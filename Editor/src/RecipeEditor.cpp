#include "RecipeEditor.h"
#include "CraftingRecipes.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
std::vector<std::pair<int, std::string>> ReadItems(const std::filesystem::path& directory)
{
    const auto path = directory / ".devilwood" / "inventory_items.json";
    if (!std::filesystem::exists(path))
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open inventory item catalog.");
    const auto data = nlohmann::json::parse(input);
    if (!data.is_object() || !data.contains("items") || !data["items"].is_array())
        throw std::runtime_error("Inventory catalog must contain an items array.");
    std::vector<std::pair<int, std::string>> items;
    for (const auto& item : data["items"]) {
        if (!item.is_object() || !item.contains("id") || !item["id"].is_number_integer() ||
            !item.contains("name") || !item["name"].is_string())
            throw std::runtime_error("Inventory catalog contains an invalid item.");
        items.emplace_back(item["id"].get<int>(), item["name"].get<std::string>());
    }
    return items;
}

bool ItemCombo(const char* label, int& id, const std::vector<std::pair<int, std::string>>& items)
{
    const auto found = std::find_if(items.begin(), items.end(),
                                    [id](const auto& item) { return item.first == id; });
    const std::string current = found == items.end() ? "None / missing" : found->second;
    bool changed = false;
    if (ImGui::BeginCombo(label, current.c_str())) {
        if (ImGui::Selectable("None", id == 0)) { id = 0; changed = true; }
        for (const auto& item : items) {
            const auto text = item.second + " (" + std::to_string(item.first) + ")";
            if (ImGui::Selectable(text.c_str(), id == item.first)) {
                id = item.first;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}
}

RecipeEditor::RecipeEditor(std::filesystem::path directory) : sceneDirectory(std::move(directory))
{
    try {
        const auto path = CraftingRecipes::Path(sceneDirectory);
        if (std::filesystem::exists(path)) {
            std::ifstream input(path, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot open crafting recipe catalog.");
            document = nlohmann::json::parse(input);
            CraftingRecipes::Parse(document);
        } else {
            document = {{"version", 1}, {"recipes", nlohmann::json::array()}};
        }
        loaded = true;
    } catch (const std::exception& e) {
        message = e.what();
        error = true;
    }
}

bool RecipeEditor::Save()
{
    if (!loaded)
        return false;
    try {
        const auto catalog = CraftingRecipes::Parse(document);
        const auto items = ReadItems(sceneDirectory);
        std::vector<int> ids;
        for (const auto& item : items) ids.push_back(item.first);
        catalog.ValidateItems(ids);
        if (dirty) {
            const auto path = CraftingRecipes::Path(sceneDirectory);
            std::filesystem::create_directories(path.parent_path());
            std::ofstream output(path, std::ios::binary);
            if (!output) throw std::runtime_error("Cannot write crafting recipe catalog.");
            output << document.dump(2) << '\n';
            if (!output) throw std::runtime_error("Failed while writing crafting recipe catalog.");
            dirty = false;
        }
        message = "Crafting recipes saved.";
        error = false;
        return true;
    } catch (const std::exception& e) {
        message = e.what();
        error = true;
        return false;
    }
}

bool RecipeEditor::SaveForRun() { return Save(); }

void RecipeEditor::Draw(bool& open)
{
    ImGui::SetNextWindowSize(ImVec2(680, 450), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Recipe Editor", &open)) { ImGui::End(); return; }
    if (!message.empty())
        ImGui::TextColored(error ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.5f, 1, 0.5f, 1),
                           "%s", message.c_str());
    if (!loaded) { ImGui::End(); return; }
    ImGui::Text("Crafting recipes%s", dirty ? " *" : "");
    if (ImGui::Button("Save")) Save();
    ImGui::SameLine();
    if (ImGui::Button("Add recipe")) {
        document["recipes"].push_back({{"name", "New recipe"},
                                        {"ingredients", nlohmann::json::array()},
                                        {"result", 0}});
        selected = document["recipes"].size() - 1;
        dirty = true;
    }
    ImGui::TextDisabled("Save inventory items before selecting them. Ingredients are unordered.");
    std::vector<std::pair<int, std::string>> items;
    try {
        items = ReadItems(sceneDirectory);
    } catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", e.what());
    }
    auto& recipes = document["recipes"];
    ImGui::BeginChild("Recipe list", ImVec2(220, 0), ImGuiChildFlags_Borders);
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(recipes[i].value("name", std::string("Recipe")).c_str(),
                              selected && *selected == i))
            selected = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Recipe details", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (selected && *selected < recipes.size()) {
        auto& recipe = recipes[*selected];
        std::array<char, 128> name{};
        const auto value = recipe.value("name", std::string{});
        std::copy_n(value.data(), std::min(value.size(), name.size() - 1), name.data());
        if (ImGui::InputText("Name", name.data(), name.size())) {
            recipe["name"] = name.data();
            dirty = true;
        }
        std::array<int, 3> ingredients{};
        for (std::size_t i = 0; i < recipe["ingredients"].size() && i < ingredients.size(); ++i)
            ingredients[i] = recipe["ingredients"][i].get<int>();
        for (int i = 0; i < 3; ++i) {
            const auto label = "Ingredient " + std::to_string(i + 1);
            if (ItemCombo(label.c_str(), ingredients[i], items)) {
                recipe["ingredients"] = nlohmann::json::array();
                for (int id : ingredients)
                    if (id < 0) recipe["ingredients"].push_back(id);
                dirty = true;
            }
        }
        int result = recipe["result"].get<int>();
        if (ItemCombo("Result", result, items)) { recipe["result"] = result; dirty = true; }
        if (ImGui::Button("Remove recipe")) {
            recipes.erase(*selected);
            selected.reset();
            dirty = true;
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
