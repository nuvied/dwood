#include "SoundManagerEditor.h"

#include "Game.hpp"
#include "ResourcesLoader.h"
#include "SoundManager.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string>

SoundManagerEditor::SoundManagerEditor()
    : manager(&Game::get_Instance().soundManager)
{
}

bool SoundManagerEditor::SaveForRun()
{
    try {
        manager->Save();
        message = "Sound manager saved.";
        messageIsError = false;
        return true;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return false;
    }
}

void SoundManagerEditor::Draw(bool& open)
{
    ImGui::SetNextWindowSize(ImVec2(760, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sound Manager", &open)) {
        ImGui::End();
        return;
    }
    if (!message.empty())
        ImGui::TextColored(messageIsError ? ImVec4(1, 0.4f, 0.4f, 1)
                                          : ImVec4(0.5f, 1, 0.5f, 1),
                           "%s", message.c_str());

    const auto availableSounds = ResourcesLoader::AvailableSounds();
    const auto availableLabel = selectedAsset ? *selectedAsset : "Choose from resource/sounds";
    if (ImGui::BeginCombo("Available sound files", availableLabel.c_str())) {
        for (const auto& asset : availableSounds)
            if (ImGui::Selectable(asset.c_str(), selectedAsset && *selectedAsset == asset))
                selectedAsset = asset;
        if (availableSounds.empty())
            ImGui::TextWrapped("Add audio files to the project's resource/sounds directory.");
        ImGui::EndCombo();
    }

    if (ImGui::Button("Save"))
        SaveForRun();
    ImGui::SameLine();
    if (ImGui::Button("Add BGM")) {
        selected = manager->AddSound(ManagedSoundType::BGM);
        if (selectedAsset) {
            auto* sound = manager->Find(*selected);
            sound->asset = *selectedAsset;
            sound->name = std::filesystem::path(*selectedAsset).stem().string();
        }
        message.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add SFX")) {
        selected = manager->AddSound(ManagedSoundType::SFX);
        if (selectedAsset) {
            auto* sound = manager->Find(*selected);
            sound->asset = *selectedAsset;
            sound->name = std::filesystem::path(*selectedAsset).stem().string();
        }
        message.clear();
    }
    ImGui::TextDisabled("BGM is exclusive; SFX instances can overlap.");

    ImGui::BeginChild("Managed sound list", ImVec2(240, 0), ImGuiChildFlags_Borders);
    for (const auto& sound : manager->Sounds()) {
        ImGui::PushID(sound.id);
        const auto label = std::string(sound.type == ManagedSoundType::BGM ? "BGM: " : "SFX: ") +
                           sound.name + "##" + std::to_string(sound.id);
        if (ImGui::Selectable(label.c_str(), selected && *selected == sound.id))
            selected = sound.id;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Managed sound details", ImVec2(0, 0), ImGuiChildFlags_Borders);
    auto* sound = selected ? manager->Find(*selected) : nullptr;
    if (!sound) {
        ImGui::TextUnformatted("Select a sound to edit its settings.");
        if (selected)
            selected.reset();
    } else {
        ImGui::Text("%s (ID %d)", sound->type == ManagedSoundType::BGM ? "Background music" : "Sound effect",
                    sound->id);
        std::array<char, 128> name{};
        std::copy_n(sound->name.begin(), std::min(sound->name.size(), name.size() - 1), name.begin());
        if (ImGui::InputText("Name", name.data(), name.size())) {
            sound->name = name.data();
            message.clear();
        }
        std::array<char, 1024> asset{};
        std::copy_n(sound->asset.begin(), std::min(sound->asset.size(), asset.size() - 1), asset.begin());
        if (ImGui::InputText("Resource path", asset.data(), asset.size())) {
            sound->asset = asset.data();
            message.clear();
        }
        if (ImGui::BeginCombo("Choose audio file", sound->asset.c_str())) {
            for (const auto& option : availableSounds)
                if (ImGui::Selectable(option.c_str(), sound->asset == option)) {
                    sound->asset = option;
                    message.clear();
                }
            if (availableSounds.empty())
                ImGui::TextWrapped("No audio files found in resource/sounds.");
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Path is relative to the project's resource directory.");
        if (ImGui::SliderFloat("Volume", &sound->volume, 0.0f, 1.0f))
            message.clear();
        if (sound->type == ManagedSoundType::BGM) {
            if (ImGui::Checkbox("Loop", &sound->loop))
                message.clear();
        } else {
            sound->loop = false;
        }
        if (ImGui::Button("Play"))
            try {
                manager->On(sound->id);
                message.clear();
            } catch (const std::exception& e) {
                message = e.what();
                messageIsError = true;
            }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            try {
                manager->Off(sound->id);
                message.clear();
            } catch (const std::exception& e) {
                message = e.what();
                messageIsError = true;
            }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            const int id = sound->id;
            manager->RemoveSound(id);
            selected.reset();
            message.clear();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
