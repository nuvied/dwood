#include "InventoryItemEditor.h"

#include "imgui.h"
#include "ResourcesLoader.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

InventoryItemEditor::InventoryItemEditor(std::filesystem::path sceneDirectory)
    : path(std::move(sceneDirectory) / ".devilwood" / "inventory_items.json")
{
    Load();
    RefreshFrames();
}

void InventoryItemEditor::Load()
{
    if (!std::filesystem::exists(path)) {
        document = nlohmann::json{{"version", 1}, {"items", nlohmann::json::array()}};
        document->at("items") = nlohmann::json::array({
            {{"id", -1}, {"name", "Bamboo stick"}, {"description", ""},
             {"recipe_tag", 1},
             {"icon", {{"atlas", "inv_items.png"}, {"frame", "stick_bamboo.png"},
                       {"source", {{"x", 0}, {"y", 78}, {"width", 26}, {"height", 26}}}}}},
            {{"id", -2}, {"name", "Broken oar"}, {"description", ""},
             {"recipe_tag", 1},
             {"icon", {{"atlas", "inv_items.png"}, {"frame", "oar.png"},
                       {"source", {{"x", 0}, {"y", 0}, {"width", 26}, {"height", 26}}}}}},
            {{"id", -3}, {"name", "Rope"}, {"description", ""},
             {"recipe_tag", 1},
             {"icon", {{"atlas", "inv_items.png"}, {"frame", "rope.png"},
                       {"source", {{"x", 0}, {"y", 52}, {"width", 26}, {"height", 26}}}}}},
            {{"id", -4}, {"name", "Oar"}, {"description", ""},
             {"recipe_tag", 3},
             {"icon", {{"atlas", "inv_items.png"}, {"frame", "oar_ready.png"},
                       {"source", {{"x", 0}, {"y", 26}, {"width", 26}, {"height", 26}}}}}}
        });
        for (auto& item : document->at("items"))
            item["carry_on_start"] = false;
        dirty = true;
        return;
    }
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open inventory item catalog.");
        auto data = nlohmann::json::parse(input);
        if (!data.is_object() || data.value("version", 0) != 1 ||
            !data.contains("items") || !data["items"].is_array())
            throw std::runtime_error("Inventory catalog must contain version 1 and an items array.");
        for (const auto& item : data["items"]) {
            if (!item.is_object() || !item.contains("id") || !item["id"].is_number_integer() ||
                !item.contains("name") || !item["name"].is_string() ||
                !item.contains("description") || !item["description"].is_string() ||
                !item.contains("icon") || !item["icon"].is_object())
                throw std::runtime_error("Inventory item is missing its internal ID or editor fields.");
            if (item.contains("carry_on_start") && !item["carry_on_start"].is_boolean())
                throw std::runtime_error("Inventory carry-on-start setting must be a boolean.");
        }
        document = std::move(data);
        dirty = false;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

void InventoryItemEditor::RefreshFrames()
{
    frames.clear();
    try {
        const std::filesystem::path atlasMetadata("resource/inv_items.json");
        std::ifstream input(atlasMetadata, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open resource/inv_items.json.");
        const auto data = nlohmann::json::parse(input);
        if (!data.contains("frames") || !data["frames"].is_object())
            throw std::runtime_error("Inventory atlas metadata has no frames.");
        for (auto frame = data["frames"].begin(); frame != data["frames"].end(); ++frame) {
            const auto& rect = frame.value()["frame"];
            if (!rect.is_object() || !rect["x"].is_number_integer() ||
                !rect["y"].is_number_integer() || !rect["w"].is_number_integer() ||
                !rect["h"].is_number_integer())
                throw std::runtime_error("Inventory atlas frame is invalid.");
            frames.push_back({
                frame.key(),
                static_cast<float>(rect["x"].get<int>()),
                static_cast<float>(rect["y"].get<int>()),
                static_cast<float>(rect["w"].get<int>()),
                static_cast<float>(rect["h"].get<int>())
            });
        }
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

void InventoryItemEditor::AddItem()
{
    if (!document)
        document = nlohmann::json{{"version", 1}, {"items", nlohmann::json::array()}};
    int nextId = -1;
    for (const auto& item : document->at("items"))
        if (item["id"].is_number_integer())
            nextId = std::min(nextId, item["id"].get<int>() - 1);
    const auto frame = frames.empty()
        ? IconFrame{"None", 0, 0, 32, 32} : frames.front();
    document->at("items").push_back({
        {"id", nextId},
        {"name", "New item"},
        {"description", ""},
        {"carry_on_start", false},
        {"icon", {
            {"atlas", "inv_items.png"},
            {"frame", frame.name},
            {"source", {{"x", frame.x}, {"y", frame.y},
                        {"width", frame.width}, {"height", frame.height}}}
        }}
    });
    selected = document->at("items").size() - 1;
    dirty = true;
}

void InventoryItemEditor::RemoveItem(std::size_t index)
{
    if (!document || index >= document->at("items").size())
        return;
    document->at("items").erase(index);
    selected.reset();
    dirty = true;
}

bool InventoryItemEditor::Save()
{
    if (!dirty)
        return true;
    try {
        if (!document)
            return true;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        if (!output)
            throw std::runtime_error("Cannot write inventory item catalog.");
        output << document->dump(2) << '\n';
        if (!output)
            throw std::runtime_error("Failed while writing inventory item catalog.");
        dirty = false;
        message = "Inventory items saved.";
        messageIsError = false;
        return true;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return false;
    }
}

bool InventoryItemEditor::SaveForRun()
{
    return Save();
}

void InventoryItemEditor::Draw(bool& open)
{
    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inventory Item Editor", &open)) {
        ImGui::End();
        return;
    }
    if (!message.empty())
        ImGui::TextColored(messageIsError ? ImVec4(1, 0.4f, 0.4f, 1)
                                          : ImVec4(0.5f, 1, 0.5f, 1),
                           "%s", message.c_str());

    ImGui::Text("Inventory items%s", dirty ? " *" : "");
    if (ImGui::Button("Save"))
        Save();
    ImGui::SameLine();
    if (ImGui::Button("Add item"))
        AddItem();

    ImGui::BeginChild("Inventory item list", ImVec2(220, 0), ImGuiChildFlags_Borders);
    if (document) {
        auto& items = document->at("items");
        for (std::size_t index = 0; index < items.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            const auto label = items[index].value("name", std::string("Unnamed item")) +
                               "##" + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), selected && *selected == index))
                selected = index;
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Inventory item details", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (selected && document && *selected < document->at("items").size()) {
        auto& item = document->at("items")[*selected];
        std::array<char, 128> name{};
        const auto nameValue = item.value("name", std::string{});
        std::copy(nameValue.begin(), nameValue.end(), name.begin());
        if (ImGui::InputText("Name", name.data(), name.size())) {
            if (std::string(name.data()).find_first_not_of(" \t\r\n") != std::string::npos) {
                item["name"] = name.data();
                dirty = true;
            }
        }

        std::array<char, 1024> description{};
        const auto descriptionValue = item.value("description", std::string{});
        std::copy(descriptionValue.begin(), descriptionValue.end(), description.begin());
        if (ImGui::InputTextMultiline("Description", description.data(), description.size(),
                                      ImVec2(-1, 120))) {
            item["description"] = description.data();
            dirty = true;
        }
        bool carryOnStart = item.value("carry_on_start", false);
        if (ImGui::Checkbox("Carry item when game starts", &carryOnStart)) {
            item["carry_on_start"] = carryOnStart;
            dirty = true;
        }

        auto& icon = item["icon"];
        const auto selectedFrame = icon.value("frame", std::string{});
        if (ImGui::BeginCombo("Icon", selectedFrame.empty() ? "Choose icon"
                                                            : selectedFrame.c_str())) {
            for (const auto& frame : frames) {
                if (ImGui::Selectable(frame.name.c_str(), frame.name == selectedFrame)) {
                    icon = {
                        {"atlas", "inv_items.png"},
                        {"frame", frame.name},
                        {"source", {{"x", frame.x}, {"y", frame.y},
                                    {"width", frame.width}, {"height", frame.height}}}
                    };
                    dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        const auto& source = icon.at("source");
        const float width = static_cast<float>(ResourcesLoader::inv_items.width);
        const float height = static_cast<float>(ResourcesLoader::inv_items.height);
        if (width > 0 && height > 0) {
            const ImVec2 preview(52, 52);
            const ImVec2 uv0(source.value("x", 0.0f) / width,
                             source.value("y", 0.0f) / height);
            const ImVec2 uv1((source.value("x", 0.0f) + source.value("width", 0.0f)) / width,
                             (source.value("y", 0.0f) + source.value("height", 0.0f)) / height);
            ImGui::Image((ImTextureID)ResourcesLoader::inv_items.id, preview, uv0, uv1);
        }
        ImGui::TextDisabled("Internal item data is managed by the game and is not shown here.");
        if (ImGui::Button("Remove item"))
            RemoveItem(*selected);
    } else {
        ImGui::TextWrapped("Add an inventory item or select one from the list.");
    }
    ImGui::EndChild();
    ImGui::End();
}
