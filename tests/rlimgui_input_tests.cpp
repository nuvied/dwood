#include "raylib.h"

#include <deque>
#include <iostream>
#include <string>

namespace {
std::deque<int> characters;

int TestGetCharPressed()
{
    if (characters.empty())
        return 0;
    const int character = characters.front();
    characters.pop_front();
    return character;
}
}

#define GetCharPressed TestGetCharPressed
#include "../Editor/include/rlImGui/rlImGui.cpp"
#undef GetCharPressed

int main()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    char name[128]{};
    ImVec2 fieldPosition;
    auto frame = [&](bool mouseDown) {
        ImGui_ImplRaylib_ProcessEvents();
        io.AddMousePosEvent(fieldPosition.x, fieldPosition.y);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Always);
        ImGui::Begin("Input test");
        ImGui::InputTextWithHint("##Name", "New variable", name, sizeof(name));
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        fieldPosition = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        ImGui::End();
        ImGui::Render();
    };

    frame(false);
    frame(false);
    frame(true);
    if (!ImGui::IsAnyItemActive() || io.WantCaptureKeyboard) {
        std::cerr << "Could not reproduce the text-field focus transition.\n";
        ImGui::DestroyContext();
        return 1;
    }

    characters = {'A', ' ', 'b'};
    frame(false);
    const bool success = std::string(name) == "A b" && characters.empty();
    if (!success)
        std::cerr << "Characters were lost after the text field gained focus: " << name << '\n';
    ImGui::DestroyContext();
    return success ? 0 : 1;
}
