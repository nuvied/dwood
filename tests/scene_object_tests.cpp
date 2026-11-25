#include "SceneObject.h"
#include "SceneDocument.h"
#include "SceneManager.h"
#include "ResourcesLoader.h"
#include "Game.hpp"
#include "Screen.h"
#include "ProjectUI.h"
#include "UI_Manager.h"
#include "SoundManager.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

Vector2 Global::mousePos = {0, 0};
Vector2 Global::lensPosition = {512, 256};
bool Global::debug = false;
bool Global::rotor_puzzle_done = false;
bool Global::oar_placed = false;
bool Global::lensOn = false;
int Global::selectedItemId = 0;

Texture2D ResourcesLoader::boat_tex_page;
Texture2D ResourcesLoader::ui_page;
Texture2D ResourcesLoader::inv_items;
Texture2D ResourcesLoader::light_blob0;
Texture2D ResourcesLoader::light_page;
Shader ResourcesLoader::basic_shader;
Shader ResourcesLoader::lenseShader;
Shader ResourcesLoader::lightOnly;
Shader ResourcesLoader::glowShader;

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

class DrawCounter : public Component
{
public:
    explicit DrawCounter(int& count) : count(count) {}
    void Update(float) override {}
    void Draw() override { ++count; }
private:
    int& count;
};

class UIStateCounter : public Component
{
public:
    UIStateCounter(int& updates, int& draws) : updates(updates), draws(draws) {}
    void Update(float) override { ++updates; }
    void Draw() override { ++draws; }
private:
    int& updates;
    int& draws;
};

class StackCounterScreen : public Screen
{
public:
    StackCounterScreen(int& updates, int& draws) : updates(updates), draws(draws) {}
    void Update(float) override { ++updates; }
    void Draw() override { ++draws; }
    void Unload() override {}
private:
    int& updates;
    int& draws;
};

class TestAction : public Action
{
public:
    TestAction(int& starts, int& finishes) : starts(starts), finishes(finishes) {}
    void OnStart() override { ++starts; }
    void Update(float) override { finished = true; }
    bool IsFinished() override { return finished; }
    void OnFinish() override { ++finishes; }
private:
    int& starts;
    int& finishes;
    bool finished = false;
};

template<typename F>
void RequireFailure(F action, const char* message)
{
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(message);
}

int TestFontGraphics()
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(256, 96, "Font test");
    const auto directory = std::filesystem::temp_directory_path() /
        ("devilwood-font-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::string error;
        const Font* font = ResourcesLoader::GetCustomFont("TestFont.ttf", &error);
        Require(font && error.empty(), "Custom font failed to load");
        Require(ResourcesLoader::GetCustomFont("TestFont.ttf") == font,
                "Custom font was not cached");
        Require(!ResourcesLoader::GetCustomFont("missing-test-font.ttf", &error) && !error.empty(),
                "Missing font was not reported");
        {
            std::ofstream invalid("resource/fonts/retry.ttf");
            invalid << "not a font";
        }
        Require(!ResourcesLoader::GetCustomFont("retry.ttf", &error) && !error.empty(),
                "Invalid font was not reported");
        std::filesystem::copy_file("resource/fonts/TestFont.ttf", "resource/fonts/retry.ttf",
                                   std::filesystem::copy_options::overwrite_existing);
        ResourcesLoader::ClearFontErrors();
        Require(ResourcesLoader::GetCustomFont("retry.ttf", &error) && error.empty(),
                "Refreshing did not retry the repaired font");

        auto data = nlohmann::json::parse(R"({"name":"Text","ui":true,"components":{"TransformComp":{"position":{"x":4,"y":4},"scale":{"x":1,"y":1}},"TextComp":{"text":"WWWiii","size":32,"font":"TestFont.ttf"}}})");
        auto object = LoadObject(data);
        auto* text = object.entity->getComponent<TextComp>();
        const auto size = text->Measure();
        Require(size.x > 0 && size.y > 0, "Custom font measured an empty string");

        auto target = LoadRenderTexture(256, 96);
        Require(IsRenderTextureValid(target), "Font test render target failed");
        BeginTextureMode(target);
        ClearBackground(BLACK);
        object.entity->Draw();
        EndTextureMode();
        auto customImage = LoadImageFromTexture(target.texture);
        data["components"]["TextComp"].erase("font");
        auto defaultObject = LoadObject(data);
        BeginTextureMode(target);
        ClearBackground(BLACK);
        defaultObject.entity->Draw();
        EndTextureMode();
        auto defaultImage = LoadImageFromTexture(target.texture);
        bool drawn = false;
        bool different = false;
        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 256; ++x) {
                const Color a = GetImageColor(customImage, x, y);
                const Color b = GetImageColor(defaultImage, x, y);
                drawn |= a.r != 0 || a.g != 0 || a.b != 0;
                different |= a.r != b.r || a.g != b.g || a.b != b.b;
            }
        }
        UnloadImage(customImage);
        UnloadImage(defaultImage);
        UnloadRenderTexture(target);
        Require(drawn && different, "Custom font did not render distinctly from the default");

        data["components"]["TextComp"]["font"] = "TestFont.ttf";
        auto scene = SceneDocument::Create(directory, "font");
        scene.AddObject();
        scene.SetObject(0, data);
        scene.Save();
        Require(SceneDocument::Open(scene.Path()).Object(0) == data,
                "Custom font did not survive scene save and reopen");
        SceneManager manager(directory);
        manager.RegisterScene(scene.Path());
        auto runtime = manager.LoadScene(scene.Path());
        Require(runtime->entities[0]->getComponent<TextComp>()->FontName() == "TestFont.ttf",
                "Runtime scene did not load the custom font");
        data["components"]["TextComp"]["font"] = "missing-test-font.ttf";
        scene.SetObject(0, data);
        scene.Save();
        RequireFailure([&] { manager.LoadScene(scene.Path()); },
                       "Runtime accepted a missing custom font");

        std::filesystem::remove_all(directory);
        ResourcesLoader::UnloadFonts();
        CloseWindow();
        std::cout << "Custom font graphics tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        ResourcesLoader::UnloadFonts();
        CloseWindow();
        std::cerr << e.what() << '\n';
        return 1;
    }
}

int TestRuntimeTextureLoading()
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Runtime texture test");
    const auto directory = std::filesystem::temp_directory_path() /
        ("devilwood-runtime-texture-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto originalResources = AssetPaths::Resource("");
    const auto originalShaders = AssetPaths::Shader("");
    try {
        const auto resourceDirectory = directory / "resource";
        std::filesystem::create_directories(resourceDirectory);
        auto image = GenImageColor(2, 2, MAGENTA);
        const auto imagePath = resourceDirectory / "runtime.png";
        const bool exported = ExportImage(image, imagePath.string().c_str());
        UnloadImage(image);
        Require(exported, "Unable to write runtime texture fixture");
        AssetPaths::Set(resourceDirectory, originalShaders);
        const auto available = ResourcesLoader::AvailableTextures();
        Require(std::find(available.begin(), available.end(), "runtime.png") != available.end(),
                "New texture was not discovered while the editor was running");
        std::string error;
        const auto* texture = ResourcesLoader::GetTexture("runtime.png", &error);
        Require(texture && IsTextureValid(*texture) && error.empty(),
                "New texture was not loaded on demand");
        Require(ResourcesLoader::GetTexture("runtime.png") == texture,
                "Runtime texture was not cached");
        ResourcesLoader::UnloadTextures();
        AssetPaths::Set(originalResources, originalShaders);
        CloseWindow();
        std::filesystem::remove_all(directory);
        std::cout << "Runtime texture loading test passed\n";
        return 0;
    } catch (const std::exception& e) {
        AssetPaths::Set(originalResources, originalShaders);
        ResourcesLoader::UnloadTextures();
        CloseWindow();
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

int TestSoundManagerPersistence()
{
    const auto directory = std::filesystem::temp_directory_path() /
        ("devilwood-sound-manager-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto originalResources = AssetPaths::Resource("");
    const auto originalShaders = AssetPaths::Shader("");
    try {
        const auto resourceDirectory = directory / "resource";
        std::filesystem::create_directories(resourceDirectory / "sounds" / "nested");
        {
            std::ofstream(resourceDirectory / "sounds" / "theme.ogg") << "test";
            std::ofstream(resourceDirectory / "sounds" / "nested" / "effect.WAV") << "test";
            std::ofstream(resourceDirectory / "sounds" / "notes.txt") << "ignore";
        }
        AssetPaths::Set(resourceDirectory, originalShaders);
        const auto availableSounds = ResourcesLoader::AvailableSounds();
        Require(availableSounds == std::vector<std::string>{
                    "sounds/nested/effect.WAV", "sounds/theme.ogg"},
                "Sound assets were not discovered recursively from resource/sounds");

        SoundManager manager;
        manager.Load(directory);
        const int id = manager.AddSound(ManagedSoundType::BGM);
        auto* sound = manager.Find(id);
        Require(sound != nullptr, "New managed sound was not found");
        sound->name = "Main theme";
        sound->asset = "music/theme.ogg";
        sound->volume = 0.4f;
        sound->loop = true;
        manager.Save();
        manager.Load(directory);
        sound = manager.Find(id);
        Require(sound && sound->name == "Main theme" &&
                sound->asset == "music/theme.ogg" &&
                std::abs(sound->volume - 0.4f) < 0.0001f &&
                sound->type == ManagedSoundType::BGM && sound->loop,
                "Managed sound settings did not survive a save and reload");

        auto actionObject = LoadObject(nlohmann::json::parse(
            R"({"name":"Sound action","components":{"InteractionComp":{"actions":[
                {"type":"SoundFadeIn","sound":1,"duration":2}
            ]}}})"));
        const auto* interaction = actionObject.entity->getComponent<InteractionComp>();
        Require(interaction && interaction->actions.size() == 1 &&
                interaction->actions[0].type == InteractionComp::ActionData::Type::SoundFadeIn &&
                interaction->actions[0].soundId == id &&
                interaction->actions[0].duration == 2.0f,
                "Managed sound interaction action was not parsed");
        RequireFailure([&] {
            manager.Sounds()[0].asset = "../outside.ogg";
            manager.Save();
        }, "Sound manager accepted an asset path outside the resource directory");
        AssetPaths::Set(originalResources, originalShaders);
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        std::cout << "Sound manager persistence test passed\n";
        return 0;
    } catch (const std::exception& e) {
        AssetPaths::Set(originalResources, originalShaders);
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

int TestRectangleGraphics()
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Rectangle test");
    RenderTexture2D target{};
    try {
        auto object = LoadObject(nlohmann::json::parse(
            R"({"name":"Rectangle","components":{"TransformComp":{"position":{"x":8,"y":8},"scale":{"x":1,"y":1}},"RectangleComp":{"width":32,"height":24,"color":{"r":200,"g":100,"b":50,"a":255},"opacity":0.5}}})"));
        target = LoadRenderTexture(64, 64);
        Require(IsRenderTextureValid(target), "Rectangle test render target failed");

        BeginTextureMode(target);
        ClearBackground(BLACK);
        object.entity->Draw();
        EndTextureMode();
        auto image = LoadImageFromTexture(target.texture);
        const Color center = GetImageColor(image, 16, 64 - 1 - 16);
        const Color outside = GetImageColor(image, 2, 64 - 1 - 2);
        UnloadImage(image);
        Require(center.r >= 98 && center.r <= 102 &&
                center.g >= 48 && center.g <= 52 &&
                center.b >= 23 && center.b <= 27 &&
                outside.r == 0 && outside.g == 0 && outside.b == 0,
                "Rectangle dimensions, color, or opacity rendered incorrectly");

        object.entity->getComponent<RectangleComp>()->opacity = 0.0f;
        BeginTextureMode(target);
        ClearBackground(BLACK);
        object.entity->Draw();
        EndTextureMode();
        image = LoadImageFromTexture(target.texture);
        const Color transparent = GetImageColor(image, 16, 64 - 1 - 16);
        UnloadImage(image);
        Require(transparent.r == 0 && transparent.g == 0 && transparent.b == 0,
                "Zero-opacity rectangle remained visible");

        auto orderedRectangles = LoadSceneGraph(nlohmann::json::parse(R"([
            {"name":"Top","components":{"RectangleComp":{"width":32,"height":24,"color":{"r":240,"g":30,"b":20,"a":255},"render_order":5}}},
            {"name":"Bottom","components":{"RectangleComp":{"width":32,"height":24,"color":{"r":20,"g":40,"b":230,"a":255},"render_order":-2}}}
        ])").get<std::vector<nlohmann::json>>());
        BeginTextureMode(target);
        ClearBackground(BLACK);
        Entity::DrawLayer(orderedRectangles.roots, false);
        EndTextureMode();
        image = LoadImageFromTexture(target.texture);
        const Color orderedCenter = GetImageColor(image, 16, 64 - 1 - 16);
        UnloadImage(image);
        Require(orderedCenter.r >= 238 && orderedCenter.g <= 32 &&
                orderedCenter.b <= 22,
                "Higher rectangle render order did not draw on top");

        UnloadRenderTexture(target);
        CloseWindow();
        std::cout << "Rectangle graphics test passed\n";
        return 0;
    } catch (const std::exception& e) {
        if (IsRenderTextureValid(target))
            UnloadRenderTexture(target);
        CloseWindow();
        std::cerr << e.what() << '\n';
        return 1;
    }
}

int TestInteractionGraphics()
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(256, 96, "Interaction test");
    const auto directory = std::filesystem::temp_directory_path() /
        ("devilwood-interaction-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto& game = Game::get_Instance();
    try {
        std::filesystem::create_directories(directory);
        const auto sourcePath = directory / "source.json";
        const auto targetPath = directory / "target.json";
        {
            std::ofstream source(sourcePath);
            source << R"({"version":1,"name":"Source","objects":[{"name":"Hotspot","components":{"TransformComp":{"position":{"x":20,"y":30},"scale":{"x":1,"y":1}},"ColliderComp":{"width":40,"height":30},"HotspotComp":{"interaction":1}}},{"name":"Interaction","components":{"InteractionComp":{"actions":[{"type":"ShowUI","ui":"status"},{"type":"ChangeScene","scene":"target.json"}]}}}]})";
            std::ofstream target(targetPath);
            target << R"({"version":1,"name":"Target","objects":[{"name":"Destination","components":{}}]})";
            Require(source && target, "Unable to write interaction test scenes");
        }
        auto sourceDocument = SceneDocument::Open(sourcePath);
        sourceDocument.SetVariables({{"source_value", "source value", 4}});
        sourceDocument.AddObject(nlohmann::json::parse(
            R"({"name":"Popup opener","components":{"InteractionComp":{"actions":[{"type":"OpenPopupScene","scene":"popup.json","duration":0.2}]}}})"));
        sourceDocument.Save();
        const auto popupPath = directory / "popup.json";
        auto popupDocument = SceneDocument::Create(directory, "popup");
        popupDocument.SetType(SceneType::Popup, "source.json");
        popupDocument.SetVariables({{"popup_value", "popup value", false}});
        popupDocument.AddObject(nlohmann::json::parse(
            R"({"name":"Popup closer","components":{"InteractionComp":{"actions":[{"type":"SetVariable","scope":"scene","id":"popup_value","value":true},{"type":"ClosePopupScene","duration":0.15}]}}})"));
        popupDocument.Save();
        std::filesystem::create_directories(directory / ".devilwood");
        {
            std::ofstream output(ProjectUI::Path(directory));
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"status","name":"Status","visible":false,"components":{}},{"id":"subtitle","name":"Subtitle","type":"subtitle","group":true,"subtitle_text":"subtitle-text","type_in_speed":2,"visible":false,"components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}}}},{"id":"subtitle-text","name":"Subtitle Text","parent":1,"components":{"TransformComp":{"position":{"x":8,"y":70},"scale":{"x":1,"y":1}},"TextComp":{"text":"Subtitle","size":12,"color":{"r":255,"g":255,"b":255,"a":255}}}},{"id":"other-text","name":"Other Text","parent":1,"components":{"TransformComp":{"position":{"x":8,"y":90},"scale":{"x":1,"y":1}},"TextComp":{"text":"Not a subtitle","size":12,"color":{"r":255,"g":255,"b":255,"a":255}}}}]})";
            Require(static_cast<bool>(output), "Unable to write graphics UI fixture");
        }
        UI_Manager uiManager;
        game.ui_m = &uiManager;
        uiManager.projectUI = ProjectUI::Load(directory);
        Require(uiManager.projectUI->IsSubtitle("subtitle") &&
                !uiManager.projectUI->IsVisible("subtitle"),
                "Subtitle group did not default to hidden");
        auto subtitleActionObject = LoadObject(nlohmann::json::parse(
            R"({"name":"Subtitle action","components":{"InteractionComp":{"actions":[{"type":"ShowSubtitle","text":"ABCD"}]}}})"));
        auto* subtitleInteraction =
            subtitleActionObject.entity->getComponent<InteractionComp>();
        Require(subtitleInteraction && subtitleInteraction->actions.size() == 1 &&
                subtitleInteraction->actions[0].type ==
                    InteractionComp::ActionData::Type::ShowSubtitle &&
                subtitleInteraction->actions[0].target.empty() &&
                subtitleInteraction->actions[0].text == "ABCD",
                "Show subtitle action did not parse its text");
        subtitleInteraction->Trigger();
        subtitleInteraction->Update(0);
        auto& projectUI = *uiManager.projectUI;
        auto* subtitleText = projectUI.GetElement("subtitle-text").getComponent<TextComp>();
        auto* otherText = projectUI.GetElement("other-text").getComponent<TextComp>();
        Require(subtitleText && projectUI.IsVisible("subtitle") &&
                projectUI.IsFading("subtitle") && projectUI.IsFadingIn("subtitle") &&
                projectUI.Alpha("subtitle") == 0.0f &&
                subtitleText->Text().empty() && otherText &&
                otherText->Text() == "Not a subtitle" && !subtitleInteraction->IsRunning(),
                "Show subtitle action did not fade in its group and start typing the message");
        projectUI.Update(0.5f);
        Require(subtitleText->Text() == "A" &&
                otherText->Text() == "Not a subtitle" &&
                !projectUI.IsFading("subtitle"),
                "Subtitle type-in speed did not reveal characters at the configured rate");
        projectUI.Update(1.5f);
        Require(subtitleText->Text() == "ABCD" &&
                otherText->Text() == "Not a subtitle" &&
                projectUI.IsVisible("subtitle") &&
                !projectUI.IsFading("subtitle"),
                "Subtitle did not finish typing before its length-based hold");
        projectUI.Update(0.59f);
        Require(!projectUI.IsFading("subtitle") && projectUI.IsVisible("subtitle"),
                "Subtitle auto-hide ignored the message-length hold duration");
        projectUI.Update(0.02f);
        Require(projectUI.IsFading("subtitle") && projectUI.IsVisible("subtitle"),
                "Subtitle did not begin its automatic fade after the length-based hold");
        projectUI.Update(0.125f);
        Require(projectUI.Alpha("subtitle") > 0.45f &&
                projectUI.Alpha("subtitle") < 0.55f,
                "Subtitle auto-hide did not fade its opacity");
        projectUI.Update(0.125f);
        Require(!projectUI.IsVisible("subtitle") && projectUI.Alpha("subtitle") == 0.0f,
                "Subtitle did not hide after the fade-out completed");
        ResourcesLoader::lenseShader = LoadShader(nullptr, "shaders/LenseShader.fs");
        game.scene_manager = std::make_unique<SceneManager>(directory);
        RequireFailure([&] { game.scene_manager->RegisterScene(popupPath); },
                       "Popup scene was registered before its main scene");
        game.scene_manager->RegisterScene(targetPath);
        game.scene_manager->RegisterScene(sourcePath);
        game.scene_manager->RegisterScene(popupPath);
        Require(game.scene_manager->MainScenes().size() == 2 &&
                game.scene_manager->PopupScenes(sourcePath) ==
                    std::vector<std::filesystem::path>{popupPath},
                "Scene manager did not group registered popup scenes by their main scene");
        RequireFailure([&] { game.scene_manager->SetStartupScene(popupPath); },
                       "Popup scene was accepted as the startup scene");
        RequireFailure([&] { game.scene_manager->UnregisterScene(sourcePath); },
                       "Main scene with a registered popup was unregistered");
        auto source = game.scene_manager->LoadScene(sourcePath);
        source->Init();
        auto* interaction = source->entities[1]->getComponent<InteractionComp>();
        game.scene_stak = std::make_unique<SceneStack>();
        game.scene_stak->addScene(std::move(source));
        Require(game.scene_stak->stack.back()->TriggerHotspotAt({25, 35}),
                "Hotspot did not receive the click");
        interaction->Update(0);
        Require(uiManager.projectUI->IsVisible("status"),
                "UI action did not run before scene transition");
        game.fm.Update(0.6f);
        Require(game.current_screen && game.current_screen->entities.size() == 1 &&
                game.current_screen->entities[0]->name == "Destination" && game.fm.fadein,
                "Hotspot did not transition to its selected scene");
        Require(uiManager.projectUI->IsVisible("status"),
                "Project UI state did not survive the scene transition");
        game.LoadManagedScene(sourcePath);
        game.variables.Set(VariableScope::Scene, "source_value", 17);
        RequireFailure([&] { game.LoadManagedScene(popupPath); },
                       "A popup scene replaced the active main scene");
        auto defaultPopupAction = LoadObject(nlohmann::json::parse(
            R"({"name":"Default transition","components":{"InteractionComp":{"actions":[{"type":"OpenPopupScene","scene":"popup.json"}]}}})"));
        Require(defaultPopupAction.entity->getComponent<InteractionComp>()->actions[0].duration ==
                    0.5f,
                "Popup transition without a duration did not retain the 0.5 second default");
        auto defaultCloseAction = LoadObject(nlohmann::json::parse(
            R"({"name":"Default close transition","components":{"InteractionComp":{"actions":[{"type":"ClosePopupScene"}]}}})"));
        Require(defaultCloseAction.entity->getComponent<InteractionComp>()->actions[0].duration ==
                    0.5f,
                "Popup close without a duration did not retain the 0.5 second default");
        auto invalidPopupAction = nlohmann::json::parse(
            R"({"name":"Invalid transition","components":{"InteractionComp":{"actions":[{"type":"ClosePopupScene","duration":-1}]}}})");
        RequireFailure([&] { LoadObject(invalidPopupAction); },
                       "Negative popup transition duration was accepted");
        auto* popupOpener = game.current_screen->entities[2]->getComponent<InteractionComp>();
        Require(popupOpener != nullptr, "Popup opener interaction was not loaded");
        popupOpener->Trigger();
        popupOpener->Update(0);
        Require(game.fm.fadeout && game.fm.duration == 0.2f &&
                game.scene_stak->stack.size() == 1,
                "Open Popup action did not start fading out before opening the popup");
        game.fm.Update(0.1f);
        Require(game.fm.fadeout && game.scene_stak->stack.size() == 1,
                "Popup transition ignored its configured fade-out duration");
        game.fm.Update(0.11f);
        Require(game.scene_stak->stack.size() == 2 &&
                game.current_screen->entities[0]->name == "Popup closer" &&
                game.fm.fadein && game.fm.duration == 0.2f &&
                !std::get<bool>(game.variables.Get(VariableScope::Scene, "popup_value")),
                "Open Popup action did not push the registered popup between fades");
        game.fm.Update(0.21f);
        popupOpener->Update(0);
        Require(!popupOpener->IsRunning(),
                "Open Popup interaction did not finish after its fade-in");
        auto* popupCloser = game.current_screen->entities[0]->getComponent<InteractionComp>();
        Require(popupCloser != nullptr, "Popup closer interaction was not loaded");
        Require(popupCloser->actions.back().duration == 0.15f,
                "Configured popup close duration was not parsed");
        popupCloser->Trigger();
        popupCloser->Update(0);
        Require(std::get<bool>(game.variables.Get(VariableScope::Scene, "popup_value")) &&
                game.scene_stak->stack.size() == 2 &&
                game.fm.fadeout && game.fm.duration == 0.15f,
                "Close Popup did not run after the scene action and start its configured fade");
        popupCloser->Update(0);
        game.fm.Update(0.16f);
        Require(game.scene_stak->stack.size() == 1 &&
                std::get<int>(game.variables.Get(VariableScope::Scene, "source_value")) == 17 &&
                !game.ClosePopupScene() && game.fm.fadein && game.fm.duration == 0.15f,
                "Close Popup did not fade out, restore parent variables, and fade back in");
        game.fm.Update(0.16f);
        Require(!game.fm.fadein && !game.fm.fadeout,
                "Close Popup transition did not complete its fade-in");
        {
            std::ofstream output(ProjectUI::Path(directory));
            output << R"({"version":1,"name":"Alpha","objects":[
                {"id":"group","name":"Group","group":true,"alpha":0.5,"components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":2,"y":2}}}},
                {"id":"sprite","name":"Sprite","parent":0,"alpha":0.5,"components":{"TransformComp":{"position":{"x":5,"y":5},"scale":{"x":6,"y":6}},"Sprite":{"atlas":"ui_page.png","source":{"x":0,"y":0,"width":1,"height":1}}}},
                {"id":"text","name":"Text","parent":0,"alpha":0.5,"components":{"TransformComp":{"position":{"x":35,"y":8},"scale":{"x":1,"y":1}},"TextComp":{"text":"TEST","size":16,"color":{"r":255,"g":255,"b":255,"a":128}}}}
            ]})";
            Require(static_cast<bool>(output), "Unable to write alpha graphics fixture");
        }
        auto pixel = GenImageColor(1, 1, WHITE);
        ResourcesLoader::ui_page = LoadTextureFromImage(pixel);
        UnloadImage(pixel);
        auto alphaUI = ProjectUI::Load(directory);
        auto alphaTarget = LoadRenderTexture(256, 96);
        Require(IsRenderTextureValid(alphaTarget), "Alpha test render target failed");
        BeginTextureMode(alphaTarget);
        ClearBackground(BLACK);
        alphaUI->Draw();
        EndTextureMode();
        auto alphaImage = LoadImageFromTexture(alphaTarget.texture);
        const auto spritePixel = GetImageColor(alphaImage, 14, 96 - 1 - 14);
        int textMaximum = 0;
        for (int y = 16; y < 70; ++y)
            for (int x = 70; x < 240; ++x)
                textMaximum = std::max(textMaximum, static_cast<int>(GetImageColor(alphaImage, x, 96 - 1 - y).r));
        UnloadImage(alphaImage);
        UnloadRenderTexture(alphaTarget);
        UnloadTexture(ResourcesLoader::ui_page);
        ResourcesLoader::ui_page = {};
        Require(spritePixel.r >= 62 && spritePixel.r <= 65,
                "Sprite did not multiply group and element alpha");
        Require(textMaximum >= 30 && textMaximum <= 34,
                "Text did not multiply group, element and text color alpha");
        game.scene_stak.reset();
        game.current_screen = nullptr;
        game.scene_manager.reset();
        game.ui_m = nullptr;
        UnloadShader(ResourcesLoader::lenseShader);
        CloseWindow();
        std::filesystem::remove_all(directory);
        std::cout << "Interaction graphics test passed\n";
        return 0;
    } catch (const std::exception& e) {
        game.scene_stak.reset();
        game.current_screen = nullptr;
        game.scene_manager.reset();
        game.ui_m = nullptr;
        UnloadShader(ResourcesLoader::lenseShader);
        CloseWindow();
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

int main(int argc, char* argv[])
{
    std::string frameError;
    const auto atlasFrames = SpriteAtlasFrames("atlas_1.png", &frameError);
    Require(!atlasFrames.empty() && frameError.empty(),
            "Sprite atlas JSON frames were not listed");
    Require(std::any_of(atlasFrames.begin(), atlasFrames.end(),
                        [](const SpriteAtlasFrame& frame) {
                            return frame.name == "Menu_bg.png" &&
                                   frame.source.width == 512 && frame.source.height == 256;
                        }),
            "Sprite atlas frame rectangle was not parsed");
    if (argc > 1 && std::string(argv[1]) == "--font-graphics")
        return TestFontGraphics();
    if (argc > 1 && std::string(argv[1]) == "--runtime-assets")
        return TestRuntimeTextureLoading();
    if (argc > 1 && std::string(argv[1]) == "--sound-manager")
        return TestSoundManagerPersistence();
    if (argc > 1 && std::string(argv[1]) == "--rectangle-graphics")
        return TestRectangleGraphics();
    if (argc > 1 && std::string(argv[1]) == "--interaction-graphics")
        return TestInteractionGraphics();
    Fade_manager instantTransition;
    int transitionStages = 0;
    instantTransition.StartFadeOut(0.0f, [&] {
        ++transitionStages;
        instantTransition.StartFadeIn(0.0f, [&] { ++transitionStages; });
    });
    instantTransition.Update(0.0f);
    Require(transitionStages == 1 && instantTransition.fadein &&
            instantTransition.alpha == 1.0f,
            "Zero-duration fade-out did not complete before fade-in");
    instantTransition.Update(0.0f);
    Require(transitionStages == 2 && !instantTransition.fadein &&
            !instantTransition.fadeout && instantTransition.alpha == 0.0f,
            "Zero-duration fade-in did not complete instantly");
    int baseUpdates = 0, popupUpdates = 0, baseDraws = 0, popupDraws = 0;
    {
        SceneStack popupStack;
        popupStack.addScene(std::make_unique<StackCounterScreen>(baseUpdates, baseDraws));
        popupStack.PushPopup(std::make_unique<StackCounterScreen>(popupUpdates, popupDraws));
        popupStack.Update(0);
        popupStack.Draw();
        Require(baseUpdates == 0 && popupUpdates == 1 &&
                baseDraws == 1 && popupDraws == 1,
                "Popup stack did not update only its top scene and draw every layer");
        Require(popupStack.ClosePopup(), "Popup stack did not close its top scene");
        popupStack.Update(0);
        Require(baseUpdates == 1 && popupUpdates == 1,
                "Closing a popup did not resume updates in the parent scene");
        Require(!popupStack.ClosePopup(), "Popup stack allowed its base scene to be closed");
    }
    auto& game = Game::get_Instance();
    {
        const auto data = nlohmann::json::parse(R"({"version":1,"recipes":[
            {"name":"Two items","ingredients":[-1,-2],"result":-3},
            {"name":"One item","ingredients":[-4],"result":-5}
        ]})");
        auto recipes = CraftingRecipes::Parse(data);
        Require(recipes.Match({-2, 0, -1}) == -3 &&
                recipes.Match({0, -4, 0}) == -5 &&
                recipes.Match({-1, 0, 0}) == 0 &&
                recipes.Match({-1, -2, -4}) == 0,
                "Crafting recipes must match exactly, independent of slot order");
        RequireFailure([&] {
            CraftingRecipes::Parse(nlohmann::json::parse(R"({"version":1,"recipes":[
                {"name":"A","ingredients":[-1,-2],"result":-3},
                {"name":"B","ingredients":[-2,-1],"result":-4}
            ]})"));
        }, "Duplicate unordered recipes must be rejected");
        RequireFailure([&] { recipes.ValidateItems({-1, -2, -3, -4}); },
                       "Missing recipe items must be rejected");
        RequireFailure([&] {
            CraftingRecipes::Parse(nlohmann::json::parse(R"({"version":1,"recipes":[
                {"name":"Repeated","ingredients":[-1,-1],"result":-3}
            ]})"));
        }, "Repeated unique items must be rejected");

        auto oldDB = game.itemDB;
        auto oldRecipes = game.craftingRecipes;
        auto oldInventory = game.runtime_inv;
        auto* oldManager = game.ui_m;
        game.ui_m = nullptr;
        game.itemDB.items_database = {
            Item("A", -1, {0, 0, 1, 1}, NONE),
            Item("B", -2, {0, 0, 1, 1}, NONE),
            Item("Result", -3, {0, 0, 1, 1}, NONE)
        };
        game.runtime_inv = {game.itemDB.getItem(-1), game.itemDB.getItem(-2), Item()};
        game.craftingRecipes = recipes;
        auto panel = LoadObject(nlohmann::json::parse(R"({"name":"Crafting","components":{
            "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":2,"y":2}},
            "CraftingComp":{}
        }})"));
        auto* crafting = panel.entity->getComponent<CraftingUIComp>();
        Require(crafting && crafting->SlotAt({55, 105}) == 0 &&
                crafting->SlotAt({135, 105}) == 1,
                "Crafting slots must follow their transform");
        game.SelectItem(-1);
        crafting->ClickAt({55, 105});
        game.SelectItem(-2);
        crafting->ClickAt({135, 105});
        Require(game.hasItem(-1) && game.hasItem(-2),
                "Crafting preview must not consume ingredients");
        crafting->ClickAt({135, 225});
        Require(!game.hasItem(-1) && !game.hasItem(-2) && game.hasItem(-3),
                "Clicking the output must consume ingredients and add the result");

        auto uiDocument = SceneDocument::Create(
            std::filesystem::temp_directory_path(),
            "crafting-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        uiDocument.AddObject(nlohmann::json::parse(R"({"id":"group","name":"Group","group":true,
            "components":{"TransformComp":{"position":{"x":5,"y":0},"scale":{"x":1,"y":1}}}})"));
        uiDocument.AddObject(nlohmann::json::parse(R"({"id":"craft","name":"Craft","parent":0,
            "components":{"TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                          "CraftingComp":{}}})"));
        auto projectUI = ProjectUI::FromDocument(uiDocument);
        game.runtime_inv = {game.itemDB.getItem(-1), game.itemDB.getItem(-2),
                            game.itemDB.getItem(-3)};
        game.SelectItem(-1);
        Require(projectUI->TriggerAt({36, 61}), "Project UI must route clicks to crafting slots");
        game.SelectItem(-2);
        projectUI->TriggerAt({76, 61});
        projectUI->TriggerAt({76, 121});
        Require(game.hasItem(-1) && game.hasItem(-2) && game.hasItem(-3),
                "Crafting must not consume inputs when the result is already owned");
        game.removeItem(-3);
        projectUI->SetVisible("group", false);
        Require(!projectUI->TriggerAt({76, 121}), "Hidden crafting groups must not receive clicks");
        projectUI->SetVisible("group", true);
        projectUI->TriggerAt({76, 121});
        Require(!game.hasItem(-1) && !game.hasItem(-2) && game.hasItem(-3),
                "Crafting in a project UI group must produce the result");
        game.itemDB = std::move(oldDB);
        game.craftingRecipes = std::move(oldRecipes);
        game.runtime_inv = std::move(oldInventory);
        game.ui_m = oldManager;
        game.item_tracker.Reset();
        Global::selectedItemId = 0;
    }
    game.SetRenderResolution(512, 256);
    Screen screen;
    screen.SetRenderResolution(game.game_width, game.game_height);
    Require(screen.cam.zoom == 2.0f && screen.ui_cam.zoom == 2.0f &&
            game.RenderScale() == 2.0f &&
            game.RenderTargetWidth() == 1024 && game.RenderTargetHeight() == 512,
            "512x256 game settings did not use a 2x render target");
    game.SetRenderResolution(1024, 512);
    screen.SetRenderResolution(game.game_width, game.game_height);
    Require(screen.cam.zoom == 4.0f && screen.ui_cam.zoom == 4.0f &&
            game.RenderScale() == 4.0f &&
            game.RenderTargetWidth() == 2048 && game.RenderTargetHeight() == 1024,
            "1024x512 game settings did not preserve 2x supersampling");

    const auto managerDirectory = std::filesystem::temp_directory_path() /
        ("devilwood-scene-manager-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto runtimeScenePath = managerDirectory / "runtime.json";
    const auto unsupportedScenePath = managerDirectory / "unsupported.json";
    const auto tooManyLightsPath = managerDirectory / "too_many_lights.json";
    try {
        int starts = 0, finishes = 0;
        ActionList actionList;
        actionList.OnFinish(nullptr);
        actionList.AddAction(std::make_unique<TestAction>(starts, finishes));
        actionList.AddAction(std::make_unique<TestAction>(starts, finishes));
        actionList.Update(0);
        Require(starts == 2 && finishes == 1 && !actionList.IsFinished(),
                "Action list did not start its first and next action");
        actionList.Update(0);
        Require(finishes == 2 && actionList.IsFinished(),
                "Action list did not finish safely without a callback");
        ResourcesLoader::boat_tex_page = {1, 512, 256, 1, 0};
        auto data = nlohmann::json::parse(R"({"name":"Door","components":{"TransformComp":{"position":{"x":12,"y":23},"scale":{"x":0.5,"y":2},"rotation":30},"Sprite":{"atlas":"atlas_1.png","source":{"x":10,"y":20,"width":30,"height":40},"offset":{"x":4,"y":-3},"opacity":0.4,"render_order":7}}})");
        auto object = LoadObject(data);
        auto* transform = object.entity->getComponent<TransformComp>();
        auto* sprite = object.entity->getComponent<Sprite>();
        Require(object.entity->name == "Door" && transform && sprite, "Game components were not created");
        Require(transform->position.x == 12 && transform->position.y == 23 &&
                transform->scale.x == 0.5f && transform->scale.y == 2 && transform->rotation == 30,
                "Transform component values are incorrect");
        Require(sprite->texture.id == 1 && sprite->src_rec.x == 10 && sprite->src_rec.y == 20 &&
                sprite->src_rec.width == 30 && sprite->src_rec.height == 40 &&
                sprite->offset.x == 4 && sprite->offset.y == -3 &&
                sprite->alpha == 0.4f &&
                sprite->drawOrder == 7 && SourceFits(*sprite),
                "Sprite component values are incorrect");
        const Vector2 spriteWorldPosition = sprite->getWorldPosition();
        Require(std::abs(spriteWorldPosition.x - 16.73205f) < 0.001f &&
                std::abs(spriteWorldPosition.y - 18.80385f) < 0.001f,
                "Sprite offset did not follow its Transform rotation and scale");

        data["components"].erase("TransformComp");
        auto spriteOnly = LoadObject(data);
        Require(!spriteOnly.entity->getComponent<TransformComp>() && spriteOnly.entity->getComponent<Sprite>(),
                "Sprite-only object did not load");
        data["components"]["Sprite"].erase("render_order");
        data["components"]["Sprite"].erase("offset");
        data["components"]["Sprite"].erase("opacity");
        auto defaultRenderOrder = LoadObject(data);
        Require(defaultRenderOrder.entity->getComponent<Sprite>()->drawOrder == 0 &&
                defaultRenderOrder.entity->getComponent<Sprite>()->offset.x == 0 &&
                defaultRenderOrder.entity->getComponent<Sprite>()->offset.y == 0 &&
                defaultRenderOrder.entity->getComponent<Sprite>()->alpha == 1,
                "Sprite without optional render settings did not use their defaults");
        data["components"]["Sprite"]["opacity"] = 1.1;
        RequireFailure([&] { LoadObject(data); }, "Invalid sprite opacity accepted");
        data["components"]["Sprite"].erase("opacity");
        data["components"].erase("Sprite");
        auto empty = LoadObject(data);
        Require(!empty.entity->getComponent<TransformComp>() && !empty.entity->getComponent<Sprite>(),
                "Empty object did not load");
        data = nlohmann::json::parse(R"({"name":"UI","components":{"TransformComp":{"position":{"x":14,"y":25},"scale":{"x":1,"y":1}},"Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":32,"height":24}},"Panel_Sprite":{"margin":4,"width":96,"height":48},"ColliderComp":{"width":20,"height":30},"CircleCol":{"radius":12},"TextComp":{"text":"Hello","size":18,"color":{"r":10,"g":20,"b":30,"a":255},"margin":{"x":2,"y":3}}}})");
        auto components = LoadObject(data);
        auto* panel = components.entity->getComponent<Panel_Sprite>();
        auto* collider = components.entity->getComponent<ColliderComp>();
        auto* circle = components.entity->getComponent<CircleCol>();
        Require(panel && panel->targetRect.width == 96 && panel->targetRect.height == 48 &&
                panel->info.left == 4, "Panel sprite values are incorrect");
        Require(collider && collider->col_rect.width == 96 && collider->col_rect.height == 48,
                "Sprite-backed collider did not use the panel bounds");
        Require(circle && circle->c.x == 14 && circle->c.y == 25 && circle->c.radius == 12,
                "Circle collider values are incorrect");
        Require(components.entity->getComponent<TextComp>() != nullptr,
                "Text component was not created");
        Require(components.entity->getComponent<TextComp>()->FontName().empty(),
                "Existing text did not use the default font");
        data["components"]["TextComp"]["font"] = "Example.TTF";
        auto customText = LoadObject(data);
        Require(customText.entity->getComponent<TextComp>()->FontName() == "Example.TTF",
                "Text component did not preserve its custom font");
        data["components"]["TextComp"]["font"] = "../outside.ttf";
        RequireFailure([&] { LoadObject(data); }, "Unsafe font path accepted");
        data["components"]["TextComp"]["font"] = "not-a-font.png";
        RequireFailure([&] { LoadObject(data); }, "Unsupported font type accepted");
        data["components"]["TextComp"]["font"] = 42;
        RequireFailure([&] { LoadObject(data); }, "Non-string font accepted");
        data["components"]["TextComp"].erase("font");

        auto rectangleData = nlohmann::json::parse(
            R"({"name":"Rectangle","components":{"TransformComp":{"position":{"x":12,"y":18},"scale":{"x":2,"y":3},"rotation":15},"RectangleComp":{"width":40,"height":24,"color":{"r":30,"g":120,"b":240,"a":200},"opacity":0.4,"render_order":7}}})");
        auto rectangleObject = LoadObject(rectangleData);
        const auto* rectangle = rectangleObject.entity->getComponent<RectangleComp>();
        Require(rectangle && rectangle->width == 40 && rectangle->height == 24 &&
                rectangle->color.r == 30 && rectangle->color.g == 120 &&
                rectangle->color.b == 240 && rectangle->color.a == 200 &&
                rectangle->opacity == 0.4f && rectangle->drawOrder == 7,
                "Colored rectangle properties were not loaded");
        rectangleData["components"]["RectangleComp"]["render_order"] = "high";
        RequireFailure([&] { LoadObject(rectangleData); },
                       "Non-integer rectangle render order was accepted");
        rectangleData["components"]["RectangleComp"]["render_order"] = 7;
        rectangleData["components"]["RectangleComp"]["opacity"] = 1.1;
        RequireFailure([&] { LoadObject(rectangleData); }, "Invalid rectangle opacity accepted");
        rectangleData["components"]["RectangleComp"]["opacity"] = 0.4;
        rectangleData["components"]["RectangleComp"]["width"] = 0;
        RequireFailure([&] { LoadObject(rectangleData); }, "Invalid rectangle width accepted");

        auto hierarchy = nlohmann::json::parse(R"([
            {"name":"Parent","components":{"TransformComp":{"position":{"x":100,"y":50},"scale":{"x":2,"y":2}}}},
            {"name":"Child","parent":0,"components":{"TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},"CircleCol":{"radius":3}}},
            {"name":"Sprite child","parent":0,"components":{"Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":16,"height":16}}}},
            {"name":"Group","parent":0,"components":{}},
            {"name":"Nested child","parent":3,"components":{"TransformComp":{"position":{"x":5,"y":6},"scale":{"x":1,"y":1}}}}
        ])").get<std::vector<nlohmann::json>>();
        auto sceneGraph = LoadSceneGraph(hierarchy);
        auto* parentTransform = sceneGraph.objects[0]->getComponent<TransformComp>();
        auto* childTransform = sceneGraph.objects[1]->getComponent<TransformComp>();
        const auto childWorld = childTransform->getWorldPosition();
        const auto childWorldScale = childTransform->getWorldScale();
        const auto* childCircle = sceneGraph.objects[1]->getComponent<CircleCol>();
        const auto* implicitTransform = sceneGraph.objects[2]->getComponent<TransformComp>();
        const auto* groupTransform = sceneGraph.objects[3]->getComponent<TransformComp>();
        const auto* nestedTransform = sceneGraph.objects[4]->getComponent<TransformComp>();
        Require(implicitTransform && groupTransform && nestedTransform &&
                sceneGraph.objects[2]->getComponent<Sprite>() &&
                SourceFits(*sceneGraph.objects[2]->getComponent<Sprite>()),
                "Parented objects without Transform components did not load");
        const auto implicitWorld = implicitTransform->getWorldPosition();
        const auto nestedWorld = nestedTransform->getWorldPosition();
        Require(sceneGraph.roots.size() == 1, "Scene graph has the wrong number of roots");
        Require(sceneGraph.objects[0]->childs.size() == 3, "Parent does not own all direct children");
        Require(sceneGraph.objects[1]->parent == sceneGraph.objects[0] &&
                childTransform->parent == parentTransform,
                "Direct child parent pointers were not set");
        Require(implicitTransform->parent == parentTransform,
                "Child without a Transform did not inherit its parent's Transform");
        Require(nestedTransform->parent == groupTransform,
                "Nested object was not attached to its grouping parent");
        Require(childWorld.x == 120 && childWorld.y == 90 &&
                childWorldScale.x == 2 && childWorldScale.y == 2 &&
                childCircle->c.x == 120 && childCircle->c.y == 90 &&
                childCircle->c.radius == 6 &&
                implicitWorld.x == 100 && implicitWorld.y == 50 &&
                nestedWorld.x == 110 && nestedWorld.y == 62,
                "Child components did not use the inherited world transform");
        auto reversedHierarchy = hierarchy;
        std::swap(reversedHierarchy[0], reversedHierarchy[1]);
        reversedHierarchy[0]["parent"] = 1;
        reversedHierarchy[1].erase("parent");
        auto reversedGraph = LoadSceneGraph(reversedHierarchy);
        Require(reversedGraph.roots.size() == 1 &&
                reversedGraph.objects[1]->childs.size() == 1 &&
                reversedGraph.objects[0]->parent == reversedGraph.objects[1],
                "Hierarchy construction depended on object order");

        data["components"].erase("Panel_Sprite");
        data["components"].erase("Sprite");
        auto standaloneCollider = LoadObject(data);
        Require(standaloneCollider.entity->getComponent<ColliderComp>()->col_rect.width == 20,
                "Standalone collider dimensions were not preserved");

        data = nlohmann::json::parse(
            R"({"name":"Point light","components":{"LightComp":{"intensity":1.5,"radius":80,"color":{"r":255,"g":200,"b":100,"a":255}}}})");
        auto pointLight = LoadObject(data);
        const auto* lightTransform = pointLight.entity->getComponent<TransformComp>();
        const auto* lightComponent = pointLight.entity->getComponent<LightComp>();
        Require(lightTransform && lightComponent && lightComponent->intensity == 1.5f &&
                lightComponent->radius == 80 && lightComponent->color.r == 255 &&
                lightComponent->color.g == 200,
                "Point light component values or implicit transform are incorrect");
        data["components"]["LightComp"]["radius"] = 0;
        RequireFailure([&] { LoadObject(data); }, "Invalid point light radius accepted");
        data = nlohmann::json::parse(
            R"({"name":"Sprite light","components":{"Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":16,"height":16},"render_order":-2},"LightComp":{"intensity":1.5,"radius":80,"use_sprite_shape":true}}})");
        auto spriteLight = LoadObject(data);
        Require(spriteLight.entity->getComponent<LightComp>()->useSpriteShape &&
                spriteLight.entity->getComponent<LightComp>()->spriteIntensity == 1.5f &&
                !spriteLight.entity->getComponent<LightComp>()->alsoUsePointLight &&
                spriteLight.entity->getComponent<Sprite>()->drawOrder == -2,
                "Sprite-shaped light or negative sprite render order was not loaded");
        data["components"]["LightComp"]["intensity"] = 2.5f;
        data["components"]["LightComp"]["sprite_intensity"] = 0.75f;
        data["components"]["LightComp"]["also_use_point_light"] = true;
        auto combinedLight = LoadObject(data);
        const auto* combinedLightComponent = combinedLight.entity->getComponent<LightComp>();
        Require(combinedLightComponent->intensity == 2.5f &&
                combinedLightComponent->spriteIntensity == 0.75f &&
                combinedLightComponent->alsoUsePointLight,
                "Combined sprite and point light settings were not kept separate");
        data["components"]["LightComp"]["sprite_intensity"] = 11;
        RequireFailure([&] { LoadObject(data); },
                       "Out-of-range sprite light intensity accepted");
        data["components"]["LightComp"]["sprite_intensity"] = 0.75;
        data["components"].erase("Sprite");
        RequireFailure([&] { LoadObject(data); },
                       "Sprite-shaped light without a Sprite component was accepted");

        auto audioObjects = nlohmann::json::parse(R"([
            {"name":"Ambience","components":{"AudioComp":{
                "asset":"bg_music.mp3","volume":0.35,"play_on_start":true
            }}},
            {"name":"Audio controls","components":{"InteractionComp":{"actions":[
                {"type":"PlayAudio","audio":0},
                {"type":"PauseAudio","audio":0},
                {"type":"StopAudio","audio":0}
            ]}}}
        ])").get<std::vector<nlohmann::json>>();
        auto audioGraph = LoadSceneGraph(audioObjects, true);
        const auto* audio = audioGraph.objects[0]->getComponent<AudioComp>();
        const auto* audioInteraction = audioGraph.objects[1]->getComponent<InteractionComp>();
        Require(audio && audio->Asset() == "bg_music.mp3" && audio->PlayOnStart() &&
                audioInteraction && audioInteraction->actions.size() == 3,
                "Audio component settings or audio actions were not loaded");
        for (const auto& action : audioInteraction->actions)
            Require(action.audio == audio, "Audio action did not resolve its target object");
        auto badAudio = audioObjects[0];
        badAudio["components"]["AudioComp"]["volume"] = 1.1;
        RequireFailure([&] { LoadObject(badAudio); }, "Out-of-range audio volume accepted");
        badAudio["components"]["AudioComp"]["volume"] = 1;
        badAudio["components"]["AudioComp"]["asset"] = "../outside.mp3";
        RequireFailure([&] { LoadObject(badAudio); }, "Unsafe audio path accepted");
        auto invalidAudioTarget = audioObjects;
        invalidAudioTarget[1]["components"]["InteractionComp"]["actions"][0]["audio"] = 1;
        RequireFailure([&] { LoadSceneGraph(invalidAudioTarget, true); },
                       "Audio action accepted an object without an Audio component");

        auto uiGraph = LoadSceneGraph(nlohmann::json::parse(R"([
            {"name":"Scene","components":{}},
            {"name":"UI","parent":0,"ui":true,"components":{}},
            {"name":"UI child","parent":1,"ui":false,"components":{}}
        ])").get<std::vector<nlohmann::json>>(), true);
        Require(!uiGraph.objects[0]->IsUiLayer() && uiGraph.objects[1]->IsUiLayer() &&
                uiGraph.objects[2]->IsUiLayer(), "UI layer inheritance is incorrect");
        int sceneDraws = 0, uiDraws = 0, childDraws = 0;
        uiGraph.objects[0]->addComponent<DrawCounter>(sceneDraws);
        uiGraph.objects[1]->addComponent<DrawCounter>(uiDraws);
        uiGraph.objects[2]->addComponent<DrawCounter>(childDraws);
        uiGraph.roots[0]->DrawLayer(false);
        Require(sceneDraws == 1 && uiDraws == 0 && childDraws == 0,
                "Scene pass rendered UI objects");
        uiGraph.roots[0]->DrawLayer(true);
        Require(sceneDraws == 1 && uiDraws == 1 && childDraws == 1,
                "UI pass did not render UI descendants exactly once");
        uiGraph.objects[1]->setActive(false);
        uiGraph.roots[0]->DrawLayer(true);
        Require(uiDraws == 1 && childDraws == 1,
                "Inactive UI parent did not hide its descendants");
        data["ui"] = "yes";
        RequireFailure([&] { LoadObject(data); }, "Invalid UI layer flag accepted");

        data = nlohmann::json::parse(R"({"name":"Door","components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":0,"y":1}},"Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":1,"height":1}}}})");
        RequireFailure([&] { LoadObject(data); }, "Invalid scale accepted");
        data["components"]["TransformComp"]["scale"]["x"] = 1;
        data["components"]["Sprite"]["source"]["x"] = std::numeric_limits<std::uint64_t>::max();
        RequireFailure([&] { LoadObject(data); }, "Out-of-range sprite coordinate accepted");
        data["components"]["Sprite"]["source"]["x"] = 0;
        data["components"]["Sprite"]["atlas"] = "../outside.png";
        RequireFailure([&] { LoadObject(data); }, "Unsafe atlas path accepted");
        data["components"]["Sprite"]["atlas"] = "missing.png";
        auto missing = LoadObject(data);
        Require(!SourceFits(*missing.entity->getComponent<Sprite>()), "Missing atlas treated as available");

        data = nlohmann::json::parse(R"({"name":"BadPanel","components":{"Panel_Sprite":{"margin":2,"width":10,"height":10}}})");
        RequireFailure([&] { LoadObject(data); }, "Panel sprite without a Sprite component accepted");
        data = nlohmann::json::parse(R"({"name":"BadText","components":{"TextComp":{"text":"Bad","size":0}}})");
        RequireFailure([&] { LoadObject(data); }, "Invalid text size accepted");

        std::filesystem::create_directories(managerDirectory);
        {
            std::ofstream output(runtimeScenePath);
            output << R"({"version":1,"name":"Runtime","settings":{"darkness":0.3},"objects":[{"name":"Point light","components":{"LightComp":{"intensity":1.5,"radius":80,"color":{"r":255,"g":200,"b":100,"a":255}}}},{"name":"UI","ui":true,"components":{"TextComp":{"text":"Hello","size":12}}},{"name":"Rectangle","components":{"TransformComp":{"position":{"x":10,"y":12},"scale":{"x":1,"y":1}},"RectangleComp":{"width":48,"height":24,"color":{"r":30,"g":120,"b":240,"a":255},"opacity":0.5}}}]})";
            if (!output)
                throw std::runtime_error("Unable to create runtime scene test file");
        }
        {
            std::ofstream output(unsupportedScenePath);
            output << R"({"version":1,"name":"Unsupported","objects":[{"name":"Bad","components":{"Future":{}}}]})";
            if (!output)
                throw std::runtime_error("Unable to create unsupported scene test file");
        }
        {
            nlohmann::json tooManyLights = {
                {"version", 1}, {"name", "Too many lights"}, {"objects", nlohmann::json::array()}
            };
            for (int i = 0; i <= RenderSettings::MaxSceneLights; ++i) {
                tooManyLights["objects"].push_back({
                    {"name", "Light " + std::to_string(i)},
                    {"components", {{"LightComp", {{"intensity", 1.0}, {"radius", 64.0}}}}}
                });
            }
            std::ofstream output(tooManyLightsPath);
            output << tooManyLights.dump();
            if (!output)
                throw std::runtime_error("Unable to create light-limit test file");
        }
        const std::vector<VariableDefinition> globalVariables = {
            {"global_flag", "enabled", false},
            {"global_state", "state", std::string("idle")}
        };
        const std::vector<VariableDefinition> sceneVariables = {
            {"scene_count", "count", 5}
        };
        VariableStore::SaveGlobalDefinitions(managerDirectory, globalVariables);
        SceneManager manager(managerDirectory);
        auto variableObjects = nlohmann::json::parse(R"([
            {"name":"Variable interaction","components":{"InteractionComp":{"actions":[
                {"type":"SetVariable","scope":"global","id":"global_flag","value":true},
                {"type":"IfElse","condition":{
                    "scope":"scene","id":"scene_count","operator":">=","value":3
                },"then":[
                    {"type":"SetVariable","scope":"global","id":"global_state","value":"high"}
                ],"else":[
                    {"type":"SetVariable","scope":"global","id":"global_state","value":"low"}
                ]}
            ]}}}
        ])").get<std::vector<nlohmann::json>>();
        auto variableGraph = LoadSceneGraph(variableObjects, true, &globalVariables,
                                            &sceneVariables);
        auto* variableInteraction =
            variableGraph.objects[0]->getComponent<InteractionComp>();
        game.variables.ResetGlobals(globalVariables);
        game.variables.ResetScene(sceneVariables);
        variableInteraction->Trigger();
        variableInteraction->Update(0);
        variableInteraction->Update(0);
        Require(std::get<bool>(game.variables.Get(VariableScope::Global, "global_flag")) &&
                std::get<std::string>(
                    game.variables.Get(VariableScope::Global, "global_state")) == "high",
                "Set Variable or true If / Else branch did not execute");
        game.variables.ResetGlobals(globalVariables);
        game.variables.ResetScene({{"scene_count", "count", 1}});
        variableInteraction->Trigger();
        variableInteraction->Update(0);
        variableInteraction->Update(0);
        Require(std::get<std::string>(
                    game.variables.Get(VariableScope::Global, "global_state")) == "low",
                "False If / Else branch did not execute");
        auto invalidVariableObjects = variableObjects;
        invalidVariableObjects[0]["components"]["InteractionComp"]["actions"][0]["id"] =
            "missing_variable";
        RequireFailure([&] {
            LoadSceneGraph(invalidVariableObjects, true, &globalVariables, &sceneVariables);
        }, "Undefined interaction variable was accepted");
        auto nestedAudioObjects = nlohmann::json::parse(R"([
            {"name":"Ambience","components":{"AudioComp":{
                "asset":"bg_music.mp3","volume":1,"play_on_start":false
            }}},
            {"name":"Conditional audio","components":{"InteractionComp":{"actions":[{
                "type":"IfElse","condition":{
                    "scope":"global","id":"global_flag","operator":"==","value":false
                },"then":[{"type":"PlayAudio","audio":0}],"else":[]
            }]}}}
        ])").get<std::vector<nlohmann::json>>();
        auto nestedAudioGraph = LoadSceneGraph(nestedAudioObjects, true, &globalVariables,
                                               &sceneVariables);
        const auto* nestedInteraction =
            nestedAudioGraph.objects[1]->getComponent<InteractionComp>();
        Require(nestedInteraction->actions[0].thenActions[0].audio ==
                    nestedAudioGraph.objects[0]->getComponent<AudioComp>(),
                "Nested audio action did not resolve its target object");
        auto variableRuntimeScene =
            SceneDocument::Create(managerDirectory, "variable_runtime");
        variableRuntimeScene.SetVariables(sceneVariables);
        variableRuntimeScene.AddObject(variableObjects[0]);
        variableRuntimeScene.Save();
        manager.RegisterScene(variableRuntimeScene.Path());
        Require(manager.LoadScene(variableRuntimeScene.Path()) != nullptr,
                "Scene manager did not validate persisted variable references");
        manager.UnregisterScene(variableRuntimeScene.Path());
        const auto itemCatalogDirectory = managerDirectory / ".devilwood";
        std::filesystem::create_directories(itemCatalogDirectory);
        {
            std::ofstream output(itemCatalogDirectory / "inventory_items.json");
            output << R"({"version":1,"items":[{
                "id":-42,"name":"Edited item","description":"A useful description.",
                "recipe_tag":1,"carry_on_start":true,
                "icon":{"atlas":"inv_items.png","frame":"rope.png",
                        "source":{"x":0,"y":52,"width":26,"height":26}},
                "internal_state":{"future":true}
            }]})";
            Require(static_cast<bool>(output), "Unable to write inventory item catalog");
        }
        ItemDB itemDatabase;
        itemDatabase.Load(managerDirectory);
        const auto editedItem = itemDatabase.getItem(-42);
        Require(editedItem.name == "Edited item" &&
                editedItem.description == "A useful description." &&
                editedItem.rect.x == 0 && editedItem.rect.y == 52 &&
                editedItem.rect.width == 26 && editedItem.rect.height == 26 &&
                editedItem.recipe_tag == OAR,
                "Inventory item editor data was not loaded into the runtime database");
        Require(editedItem.carryOnStart,
                "Inventory item carry-on-start setting was not loaded");
        Require(!ProjectUI::Load(managerDirectory), "Project UI should be optional");
        const auto uiPath = ProjectUI::Path(managerDirectory);
        std::filesystem::create_directories(uiPath.parent_path());
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"status","name":"Status","visible":false,"enabled":true,"components":{}}]})";
            Require(static_cast<bool>(output), "Unable to write project UI test fixture");
        }
        auto projectUI = ProjectUI::Load(managerDirectory);
        Require(projectUI && projectUI->HasElement("status") && !projectUI->IsVisible("status") &&
                projectUI->IsEnabled("status"), "Project UI defaults were not loaded");
        int projectUiUpdates = 0, projectUiDraws = 0;
        projectUI->GetElement("status").addComponent<UIStateCounter>(projectUiUpdates, projectUiDraws);
        projectUI->Update(0);
        projectUI->Draw();
        Require(projectUiUpdates == 1 && projectUiDraws == 0, "Hidden UI should update but not draw");
        projectUI->SetVisible("status", true);
        projectUI->SetEnabled("status", false);
        projectUI->Update(0);
        projectUI->Draw();
        Require(projectUiUpdates == 1 && projectUiDraws == 1, "Disabled UI should draw but not update");
        {
            auto inventoryAction = LoadObject(nlohmann::json::parse(
                R"({"name":"Give item","components":{"InteractionComp":{"actions":[
                    {"type":"AddItem","item":-42}
                ]}}})"));
            game.itemDB = itemDatabase;
            game.runtime_inv.assign(3, Item());
            UI_Manager actionManager;
            game.ui_m = &actionManager;
            auto* interaction = inventoryAction.entity->getComponent<InteractionComp>();
            interaction->Trigger();
            interaction->Update(0);
            Require(game.runtime_inv[0].id == -42,
                    "Add Item action did not add the database item to runtime inventory");
            auto removeAction = LoadObject(nlohmann::json::parse(
                R"({"name":"Remove item","components":{"InteractionComp":{"actions":[
                    {"type":"RemoveItem","item":-42}
                ]}}})"));
            auto* removeInteraction = removeAction.entity->getComponent<InteractionComp>();
            removeInteraction->Trigger();
            removeInteraction->Update(0);
            Require(game.runtime_inv[0].id == 0,
                    "Remove Item action did not remove the item from runtime inventory");
            game.ui_m = nullptr;
        }
        {
            auto startupInteraction = LoadObject(nlohmann::json::parse(
                R"({"name":"Startup inventory","components":{"InteractionComp":{
                    "run_on_scene_start":true,
                    "actions":[{"type":"AddItem","item":-42}]
                }}})"));
            auto* interaction = startupInteraction.entity->getComponent<InteractionComp>();
            Require(interaction && interaction->runOnSceneStart,
                    "Run-on-scene-start option was not loaded");
            UI_Manager startupManager;
            game.ui_m = &startupManager;
            game.itemDB = itemDatabase;
            game.runtime_inv.assign(2, Item());
            Screen startupScreen;
            startupScreen.addEntity(std::move(startupInteraction.entity));
            startupScreen.SceneLoaded();
            startupScreen.entities[0]->getComponent<InteractionComp>()->Update(0);
            Require(game.runtime_inv[0].id == -42,
                    "Run-on-scene-start interaction did not add its item");
            game.ui_m = nullptr;
        }
        {
            auto inventoryUI = LoadObject(nlohmann::json::parse(
                R"({"name":"Inventory","components":{
                    "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                    "InventoryComp":{"slot_count":5,"layout":"vertical","spacing":3,
                        "icon_size":{"width":20,"height":24},
                        "slot_atlas":"ui_page.png",
                        "slot_source":{"x":0,"y":0,"width":32,"height":32}}
                }})"));
            Require(inventoryUI.entity->getComponent<InventoryUIComp>(),
                    "Inventory UI component was not loaded");
            auto invalidInventory = nlohmann::json::parse(
                R"({"name":"Inventory","components":{"InventoryComp":{
                    "slot_count":0,"layout":"horizontal","spacing":0,
                    "slot_atlas":"ui_page.png",
                    "slot_source":{"x":0,"y":0,"width":32,"height":32}}
                }})");
            RequireFailure([&] { LoadObject(invalidInventory); },
                           "Inventory UI accepted an empty slot layout");
        }
        {
            auto pickupObject = LoadObject(nlohmann::json::parse(
                R"({"name":"Pickup item","components":{
                    "TransformComp":{"position":{"x":50,"y":40},"scale":{"x":2,"y":2}},
                    "Sprite":{"atlas":"inv_items.png","source":{"x":0,"y":0,"width":20,"height":10}},
                    "InventoryItemComp":{"item":-42}
                }})"));
            auto* pickup = pickupObject.entity->getComponent<InventoryItemComp>();
            Require(pickup && pickup->itemId == -42,
                    "InventoryItemComp was not loaded with its item ID");
            Require(pickup->Contains({60, 45}) && !pickup->Contains({45, 45}) &&
                    !pickup->Contains({95, 45}),
                    "InventoryItemComp hit testing did not match its scaled Sprite bounds");

            RequireFailure([&] {
                LoadObject(nlohmann::json::parse(
                    R"({"name":"Missing sprite","components":{"InventoryItemComp":{"item":-42}}})"));
            }, "InventoryItemComp should require a Sprite component");

            RequireFailure([&] {
                LoadSceneGraph({nlohmann::json::parse(
                    R"({"name":"Unset item","components":{
                        "Sprite":{"atlas":"inv_items.png","source":{"x":0,"y":0,"width":20,"height":10}},
                        "InventoryItemComp":{"item":0}
                    }})")}, true);
            }, "Runtime scene should reject InventoryItemComp with unset item ID");

            UI_Manager pickupManager;
            game.ui_m = &pickupManager;
            game.itemDB = itemDatabase;
            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());

            Screen pickupScreen;
            pickupScreen.addEntity(std::move(pickupObject.entity));
            pickupScreen.SceneLoaded();
            Require(pickupScreen.entities[0]->isActive(),
                    "Uncollected inventory item should remain active on scene load");
            Require(!pickupScreen.TriggerHotspotAt({10, 10}),
                    "Clicking outside an inventory item should not collect it");
            Require(game.runtime_inv[0].id == 0,
                    "Inventory should remain empty after clicking outside item");
            Require(pickupScreen.TriggerHotspotAt({60, 45}),
                    "Clicking an inventory item should trigger collection");
            Require(game.runtime_inv[0].id == -42,
                    "Clicking an inventory item did not add the item to runtime inventory");
            Require(pickup->IsCollecting() && pickupScreen.entities[0]->isActive(),
                    "Inventory item should fade out before deactivating");
            Require(std::abs(game.InventoryItemAlpha(-42) - 0.0f) < 0.0001f,
                    "Newly added inventory item should start fade-in at alpha 0");
            Require(!pickupScreen.TriggerHotspotAt({60, 45}),
                    "Inventory item should ignore clicks while fading out");
            pickupScreen.entities[0]->Update(0.05f);
            game.UpdateInventoryItemFades(0.05f);
            Require(std::abs(pickupScreen.entities[0]->getComponent<Sprite>()->alpha - 0.5f) < 0.0001f &&
                    pickupScreen.entities[0]->isActive(),
                    "Inventory item sprite should be halfway faded out at 0.05s");
            Require(std::abs(game.InventoryItemAlpha(-42) - 0.5f) < 0.0001f,
                    "Inventory UI item should be halfway faded in at 0.05s");
            pickupScreen.entities[0]->Update(0.05f);
            game.UpdateInventoryItemFades(0.05f);
            Require(pickupScreen.entities[0]->getComponent<Sprite>()->alpha == 0.0f &&
                    !pickupScreen.entities[0]->isActive(),
                    "Collected inventory item should finish fade-out and deactivate at 0.1s");
            Require(std::abs(game.InventoryItemAlpha(-42) - 1.0f) < 0.0001f,
                    "Inventory UI item should finish fade-in at 0.1s");
            Require(game.item_tracker.hasItem(-42),
                    "Collected inventory item was not recorded in item_tracker");

            auto reloadedPickup = LoadObject(nlohmann::json::parse(
                R"({"name":"Pickup item","components":{
                    "TransformComp":{"position":{"x":50,"y":40},"scale":{"x":2,"y":2}},
                    "Sprite":{"atlas":"inv_items.png","source":{"x":0,"y":0,"width":20,"height":10}},
                    "InventoryItemComp":{"item":-42}
                }})"));
            Screen reloadScreen;
            reloadScreen.addEntity(std::move(reloadedPickup.entity));
            reloadScreen.SceneLoaded();
            Require(!reloadScreen.entities[0]->isActive(),
                    "Already collected inventory item should stay inactive when scene reloads");

            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            game.ui_m = nullptr;
        }
        {
            const std::vector<VariableDefinition> hotspotGlobals = {
                {"has_key", "Has Key", false}
            };
            const std::vector<VariableDefinition> hotspotSceneVars = {};
            const std::vector<nlohmann::json> hotspotObjects = {
                nlohmann::json::parse(R"({"name":"Unlock","components":{"InteractionComp":{
                    "actions":[{"type":"AddItem","item":-42}]
                }}})"),
                nlohmann::json::parse(R"({"name":"Locked door","components":{
                    "TransformComp":{"position":{"x":10,"y":10},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "HotspotComp":{
                        "interaction":0,
                        "check_variable":true,
                        "variable":{"scope":"global","id":"has_key"},
                        "fail_message":"The door is locked."
                    }
                }})")
            };
            auto hotspotGraph = LoadSceneGraph(
                hotspotObjects, true, &hotspotGlobals, &hotspotSceneVars);
            RequireFailure([&] {
                LoadSceneGraph(hotspotObjects, true, &hotspotSceneVars, &hotspotSceneVars);
            }, "Hotspot variable check should reject an undefined variable");

            UI_Manager hotspotUIManager;
            const auto hotspotUIDir = managerDirectory / "hotspot_ui";
            const auto hotspotUIPath = ProjectUI::Path(hotspotUIDir);
            std::filesystem::create_directories(hotspotUIPath.parent_path());
            {
                std::ofstream output(hotspotUIPath);
                output << R"({"version":1,"name":"Project UI","objects":[
                    {"id":"sub_group","name":"Subtitle","group":true,"type":"subtitle",
                     "subtitle_text":"sub_text","type_in_speed":1000,
                     "components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}}}},
                    {"id":"sub_text","name":"Text","parent":0,
                     "components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                                   "TextComp":{"text":"","size":16}}}
                ]})";
            }
            hotspotUIManager.projectUI = ProjectUI::Load(hotspotUIDir);
            game.ui_m = &hotspotUIManager;
            game.itemDB = itemDatabase;
            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            game.variables.ResetGlobals(hotspotGlobals);
            game.variables.ResetScene(hotspotSceneVars);

            Screen hotspotScreen;
            for (auto& root : hotspotGraph.roots)
                hotspotScreen.addEntity(std::move(root));
            hotspotScreen.SceneLoaded();

            Require(hotspotScreen.TriggerHotspotAt({20, 20}),
                    "Clicking variable-checked hotspot should consume the click");
            hotspotScreen.Update(0.05f);
            hotspotUIManager.projectUI->Update(0.5f);
            Require(game.runtime_inv[0].id == 0,
                    "Hotspot should not trigger success interaction when variable is false");
            Require(hotspotUIManager.projectUI->IsVisible("sub_group") &&
                    hotspotUIManager.projectUI->GetElement("sub_text")
                        .getComponent<TextComp>()->Text() == "The door is locked.",
                    "Hotspot should display fail_message subtitle when variable is false");

            game.variables.Set(VariableScope::Global, "has_key", true);
            Require(hotspotScreen.TriggerHotspotAt({20, 20}),
                    "Clicking variable-checked hotspot when true should trigger interaction");
            hotspotScreen.Update(0.05f);
            Require(game.runtime_inv[0].id == -42,
                    "Hotspot should trigger success interaction when variable is true");

            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            game.ui_m = nullptr;
        }
        {
            const std::vector<VariableDefinition> itemHotspotGlobals = {
                {"door_opened", "Door Opened", false}
            };
            const std::vector<VariableDefinition> itemHotspotSceneVars = {};
            const std::vector<nlohmann::json> itemHotspotObjects = {
                nlohmann::json::parse(R"({"name":"OpenDoor","components":{"InteractionComp":{
                    "actions":[{"type":"SetVariable","scope":"global","id":"door_opened","value":true}]
                }}})"),
                nlohmann::json::parse(R"({"name":"Keyhole","components":{
                    "TransformComp":{"position":{"x":100,"y":100},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "HotspotComp":{
                        "interaction":0,
                        "require_item":true,
                        "item":-42,
                        "consume_item":true,
                        "fail_message":"Needs a key."
                    }
                }})")
            };
            auto itemHotspotGraph = LoadSceneGraph(
                itemHotspotObjects, true, &itemHotspotGlobals, &itemHotspotSceneVars);
            RequireFailure([&] {
                LoadSceneGraph({nlohmann::json::parse(R"({"name":"BadKeyhole","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":20,"height":20},
                    "HotspotComp":{"interaction":null,"require_item":true,"item":0}
                }})")}, true, &itemHotspotGlobals, &itemHotspotSceneVars);
            }, "Item-checked hotspot should reject an unset item ID");

            UI_Manager invUIManager;
            const auto invUIDir = managerDirectory / "selectable_inv_ui";
            const auto invUIPath = ProjectUI::Path(invUIDir);
            std::filesystem::create_directories(invUIPath.parent_path());
            {
                std::ofstream output(invUIPath);
                output << R"({"version":1,"name":"Project UI","objects":[
                    {"id":"sub_group","name":"Subtitle","group":true,"type":"subtitle",
                     "subtitle_text":"sub_text","type_in_speed":1000,
                     "components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}}}},
                    {"id":"sub_text","name":"Text","parent":0,
                     "components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                                   "TextComp":{"text":"","size":16}}},
                    {"id":"inv_bar","name":"Inventory","components":{
                        "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                        "InventoryComp":{"slot_count":3,"layout":"horizontal","spacing":4,
                            "icon_size":{"width":20,"height":20},
                            "slot_atlas":"ui_page.png",
                            "slot_source":{"x":0,"y":0,"width":32,"height":32}}
                    }}
                ]})";
            }
            invUIManager.projectUI = ProjectUI::Load(invUIDir);
            game.ui_m = &invUIManager;
            game.itemDB = itemDatabase;
            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            Global::selectedItemId = 0;
            game.variables.ResetGlobals(itemHotspotGlobals);
            game.variables.ResetScene(itemHotspotSceneVars);
            game.AddItem(-42);

            Screen itemScreen;
            for (auto& root : itemHotspotGraph.roots)
                itemScreen.addEntity(std::move(root));
            itemScreen.SceneLoaded();

            // Clicking hotspot before selecting item fails and displays fail_message
            Require(itemScreen.TriggerHotspotAt({110, 110}),
                    "Clicking item-checked hotspot should consume the click");
            itemScreen.Update(0.05f);
            invUIManager.projectUI->Update(0.5f);
            Require(!std::get<bool>(game.variables.Get(VariableScope::Global, "door_opened")),
                    "Hotspot should not succeed when required item is not selected");
            Require(invUIManager.projectUI->GetElement("sub_text")
                        .getComponent<TextComp>()->Text() == "Needs a key.",
                    "Hotspot should display fail_message when required item is not selected");

            // Clicking slot 0 in ProjectUI selects item -42, clicking again deselects
            Global::mousePos = {20, 30};
            Require(invUIManager.projectUI->IsUnderMouse(),
                    "InventoryUIComp should report mouse hover over a slot");
            Require(invUIManager.projectUI->TriggerAt({20, 30}) && Global::selectedItemId == -42,
                    "Clicking an occupied inventory slot should select its item");
            Require(invUIManager.projectUI->TriggerAt({20, 30}) && Global::selectedItemId == 0,
                    "Clicking an already-selected inventory slot should deselect it");
            // Re-select slot 0 and click empty slot 1 -> deselects
            Require(invUIManager.projectUI->TriggerAt({20, 30}) && Global::selectedItemId == -42,
                    "Clicking slot 0 should re-select item -42");
            Require(invUIManager.projectUI->TriggerAt({56, 30}) && Global::selectedItemId == 0,
                    "Clicking an empty inventory slot should deselect the active item");

            // Select item -42 and use it on the hotspot -> succeeds and consumes item
            Require(invUIManager.projectUI->TriggerAt({20, 30}) && Global::selectedItemId == -42,
                    "Selecting item -42 before using on hotspot failed");
            Require(itemScreen.TriggerHotspotAt({110, 110}),
                    "Clicking item-checked hotspot with required item should succeed");
            itemScreen.Update(0.05f);
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "door_opened")),
                    "Hotspot should trigger success interaction when required item is selected");
            Require(!game.hasItem(-42) && Global::selectedItemId == 0,
                    "Consume-on-success hotspot should remove item from inventory and clear selection");

            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            Global::selectedItemId = 0;
            game.ui_m = nullptr;
        }
        {
            const std::vector<VariableDefinition> multiItemGlobals = {
                {"used_default", "Used Default", false},
                {"used_item_a", "Used Item A", false},
                {"used_item_b", "Used Item B", false}
            };
            const std::vector<VariableDefinition> multiItemSceneVars = {};
            const std::vector<nlohmann::json> multiItemObjects = {
                nlohmann::json::parse(R"({"name":"DefaultAction","components":{"InteractionComp":{
                    "actions":[{"type":"SetVariable","scope":"global","id":"used_default","value":true}]
                }}})"),
                nlohmann::json::parse(R"({"name":"ItemAAction","components":{"InteractionComp":{
                    "actions":[{"type":"SetVariable","scope":"global","id":"used_item_a","value":true}]
                }}})"),
                nlohmann::json::parse(R"({"name":"ItemBAction","components":{"InteractionComp":{
                    "actions":[{"type":"SetVariable","scope":"global","id":"used_item_b","value":true}]
                }}})"),
                nlohmann::json::parse(R"({"name":"MultiHotspot","components":{
                    "TransformComp":{"position":{"x":10,"y":10},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "HotspotComp":{
                        "interaction":0,
                        "item_interactions":[
                            {"item":-1,"interaction":1,"consume_item":false},
                            {"item":-2,"interaction":2,"consume_item":true,"disable_on_success":true}
                        ]
                    }
                }})")
            };
            auto multiGraph = LoadSceneGraph(
                multiItemObjects, true, &multiItemGlobals, &multiItemSceneVars);
            RequireFailure([&] {
                LoadSceneGraph({nlohmann::json::parse(R"({"name":"BadMulti","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":20,"height":20},
                    "HotspotComp":{"interaction":null,"item_interactions":[{"item":0,"interaction":null}]}
                }})")}, true, &multiItemGlobals, &multiItemSceneVars);
            }, "Hotspot item interaction should reject an unset item ID");

            ItemDB multiDB;
            multiDB.items_database = {
                Item("key_a", -1, {0, 0, 26, 26}, NONE),
                Item("key_b", -2, {0, 26, 26, 26}, NONE)
            };
            game.itemDB = multiDB;
            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            Global::selectedItemId = 0;
            game.variables.ResetGlobals(multiItemGlobals);
            game.variables.ResetScene(multiItemSceneVars);
            game.AddItem(-1);
            game.AddItem(-2);

            Screen multiScreen;
            for (auto& root : multiGraph.roots)
                multiScreen.addEntity(std::move(root));
            multiScreen.SceneLoaded();

            // No item selected -> runs default interaction
            Require(multiScreen.TriggerHotspotAt({20, 20}),
                    "Default click on multi-item hotspot should trigger");
            multiScreen.Update(0.05f);
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "used_default")) &&
                    !std::get<bool>(game.variables.Get(VariableScope::Global, "used_item_a")) &&
                    !std::get<bool>(game.variables.Get(VariableScope::Global, "used_item_b")),
                    "Hotspot without selected item should run default interaction");

            // Select item -1 (non-consuming) -> runs ItemAAction and keeps item -1
            game.SelectItem(-1);
            Require(multiScreen.TriggerHotspotAt({20, 20}),
                    "Click with item -1 should trigger item interaction A");
            multiScreen.Update(0.05f);
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "used_item_a")) &&
                    game.hasItem(-1) && Global::selectedItemId == -1,
                    "Non-consuming item interaction should run and keep item");

            // Select item -2 (consuming + disable_on_success) -> runs ItemBAction, consumes item -2, disables hotspot
            game.SelectItem(-2);
            Require(multiScreen.TriggerHotspotAt({20, 20}),
                    "Click with item -2 should trigger item interaction B");
            multiScreen.Update(0.05f);
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "used_item_b")) &&
                    !game.hasItem(-2) && Global::selectedItemId == 0,
                    "Consuming item interaction should run and remove item");
            Require(!multiScreen.TriggerHotspotAt({20, 20}),
                    "Hotspot with disable_on_success should ignore further clicks after success");

            // Verify SceneDocument::RemoveObject adjusts item_interactions indices
            auto doc = SceneDocument::Create(managerDirectory, "hotspot_item_refs");
            for (const auto& obj : multiItemObjects)
                doc.AddObject(obj);
            doc.RemoveObject(1); // removes ItemAAction; ItemBAction shifts 2 -> 1
            const auto updatedHotspot = doc.Object(2)["components"]["HotspotComp"];
            Require(updatedHotspot["item_interactions"][0]["interaction"].is_null() &&
                    updatedHotspot["item_interactions"][1]["interaction"].get<int>() == 1,
                    "Removing an object did not update hotspot item_interactions indices");

            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            Global::selectedItemId = 0;
        }
        {
            const std::vector<VariableDefinition> actionGlobals = {
                {"switch_on", "Switch On", false},
                {"after_change", "After Change", false}
            };
            const std::vector<VariableDefinition> actionSceneVars = {};
            const std::vector<nlohmann::json> actionObjects = {
                nlohmann::json::parse(R"({"name":"Prop","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "Sprite":{"atlas":"inv_items.png","source":{"x":0,"y":0,"width":20,"height":20}}
                }})"),
                nlohmann::json::parse(R"({"name":"DoorHotspot","components":{
                    "TransformComp":{"position":{"x":10,"y":10},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "HotspotComp":{"interaction":null}
                }})"),
                nlohmann::json::parse(R"({"name":"NewInteraction","components":{"InteractionComp":{
                    "actions":[{"type":"SetVariable","scope":"global","id":"after_change","value":true}]
                }}})"),
                nlohmann::json::parse(R"({"name":"HideProp","components":{"InteractionComp":{
                    "actions":[{"type":"HideObject","object":0}]
                }}})"),
                nlohmann::json::parse(R"({"name":"ShowProp","components":{"InteractionComp":{
                    "actions":[{"type":"ShowObject","object":0}]
                }}})"),
                nlohmann::json::parse(R"({"name":"UpdateDoor","components":{"InteractionComp":{
                    "actions":[{"type":"ChangeHotspotSetting","hotspot":1,"interaction":2,
                                "check_variable":true,"variable":{"scope":"global","id":"switch_on"},
                                "require_item":false,"item":0,"consume_item":false,
                                "disable_on_success":true,"fail_message":"Switch is off."}]
                }}})"),
                nlohmann::json::parse(R"({"name":"DisableDoor","components":{"InteractionComp":{
                    "actions":[{"type":"DisableHotspot","hotspot":1}]
                }}})")
            };
            auto actionGraph = LoadSceneGraph(
                actionObjects, true, &actionGlobals, &actionSceneVars);
            game.variables.ResetGlobals(actionGlobals);
            game.variables.ResetScene(actionSceneVars);

            Screen actionScreen;
            for (auto& root : actionGraph.roots)
                actionScreen.addEntity(std::move(root));
            actionScreen.SceneLoaded();

            // HideObject / ShowObject
            actionScreen.entities[3]->getComponent<InteractionComp>()->Trigger();
            actionScreen.Update(0.05f);
            Require(!actionScreen.entities[0]->isActive(),
                    "HideObject action did not hide the target scene object");
            actionScreen.entities[4]->getComponent<InteractionComp>()->Trigger();
            actionScreen.Update(0.05f);
            Require(actionScreen.entities[0]->isActive(),
                    "ShowObject action did not show the target scene object");

            // ChangeHotspotSetting
            actionScreen.entities[5]->getComponent<InteractionComp>()->Trigger();
            actionScreen.Update(0.05f);
            const auto* doorHotspot = actionScreen.entities[1]->getComponent<HotspotComp>();
            Require(doorHotspot && doorHotspot->checkVariable &&
                    doorHotspot->variableId == "switch_on" &&
                    doorHotspot->disableOnSuccess &&
                    doorHotspot->failMessage == "Switch is off." &&
                    doorHotspot->interaction == actionScreen.entities[2]->getComponent<InteractionComp>(),
                    "ChangeHotspotSetting action did not update hotspot settings");

            // Triggering updated hotspot when switch_on is true runs NewInteraction & disables hotspot
            game.variables.Set(VariableScope::Global, "switch_on", true);
            Require(actionScreen.TriggerHotspotAt({20, 20}),
                    "Updated hotspot should trigger on click");
            actionScreen.Update(0.05f);
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "after_change")) &&
                    !actionScreen.entities[1]->isActive(),
                    "Updated hotspot did not run new interaction and disable itself");

            // Re-enable door and test DisableHotspot action
            actionScreen.entities[1]->setActive(true);
            actionScreen.entities[6]->getComponent<InteractionComp>()->Trigger();
            actionScreen.Update(0.05f);
            Require(!actionScreen.entities[1]->isActive(),
                    "DisableHotspot action did not disable the target hotspot");

            // Verify SceneDocument::RemoveObject updates object/hotspot/interaction indices
            auto actionDoc = SceneDocument::Create(managerDirectory, "action_refs");
            for (const auto& obj : actionObjects)
                actionDoc.AddObject(obj);
            actionDoc.RemoveObject(0); // removes Prop; DoorHotspot 1->0, NewInteraction 2->1
            Require(actionDoc.Object(2)["components"]["InteractionComp"]["actions"][0]["object"].is_null(),
                    "Removing target object did not nullify HideObject reference");
            const auto updatedSetting =
                actionDoc.Object(4)["components"]["InteractionComp"]["actions"][0];
            Require(updatedSetting["hotspot"].get<int>() == 0 &&
                    updatedSetting["interaction"].get<int>() == 1,
                    "Removing an earlier object did not shift ChangeHotspotSetting indices");
        }
        {
            const auto scriptDir = managerDirectory / "scripts";
            std::filesystem::create_directories(scriptDir);
            AssetPaths::Set(AssetPaths::Resource(""), AssetPaths::Shader(""), scriptDir);
            {
                std::ofstream out(scriptDir / "mover.lua", std::ios::binary | std::ios::trunc);
                out << "local speed = 100\n"
                    << "function on_start(self)\n"
                    << "    vars.set_global('lua_started', true)\n"
                    << "end\n"
                    << "function on_update(self, dt)\n"
                    << "    self:set_position(self:get_x() + speed * dt, self:get_y())\n"
                    << "end\n"
                    << "function on_click(self)\n"
                    << "    vars.set_global('lua_clicks', vars.get_global('lua_clicks') + 1)\n"
                    << "end\n"
                    << "function give_bonus(self)\n"
                    << "    vars.set_global('lua_clicks', vars.get_global('lua_clicks') + 10)\n"
                    << "end\n";
            }
            const std::vector<VariableDefinition> luaGlobals = {
                {"lua_started", "Lua Started", false},
                {"lua_clicks", "Lua Clicks", 0}
            };
            const std::vector<VariableDefinition> luaSceneVars = {};
            const std::vector<nlohmann::json> luaObjects = {
                nlohmann::json::parse(R"({"name":"ScriptedBox","components":{
                    "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                    "RectangleComp":{"width":40,"height":40},
                    "ScriptComp":{"script":"mover.lua"}
                }})"),
                nlohmann::json::parse(R"({"name":"RunScriptObj","components":{"InteractionComp":{
                    "actions":[{"type":"RunScript","script":"mover.lua","function":"give_bonus"}]
                }}})")
            };
            game.scriptEngine.Reset();
            game.variables.ResetGlobals(luaGlobals);
            game.variables.ResetScene(luaSceneVars);

            auto luaGraph = LoadSceneGraph(luaObjects, true, &luaGlobals, &luaSceneVars);
            // LoadSceneGraph calls Update(0), which must NOT execute Lua scripts in editor preview
            Require(!std::get<bool>(game.variables.Get(VariableScope::Global, "lua_started")),
                    "ScriptComp should not run on_start during LoadSceneGraph editor preview");

            Screen luaScreen;
            for (auto& root : luaGraph.roots)
                luaScreen.addEntity(std::move(root));
            luaScreen.SceneLoaded();
            Require(std::get<bool>(game.variables.Get(VariableScope::Global, "lua_started")),
                    "ScriptComp on_start did not run on SceneLoaded");

            luaScreen.Update(0.5f);
            const auto* boxTransform = luaScreen.entities[0]->getComponent<TransformComp>();
            Require(std::abs(boxTransform->position.x - 60.0f) < 0.01f,
                    "ScriptComp on_update did not move entity");

            Require(luaScreen.TriggerHotspotAt({70, 30}),
                    "Clicking a ScriptComp entity with on_click should trigger it");
            Require(std::get<int>(game.variables.Get(VariableScope::Global, "lua_clicks")) == 1,
                    "ScriptComp on_click did not update variable");

            luaScreen.entities[1]->getComponent<InteractionComp>()->Trigger();
            luaScreen.Update(0.0f);
            Require(std::get<int>(game.variables.Get(VariableScope::Global, "lua_clicks")) == 11,
                    "RunScript interaction action did not invoke target Lua function");

            RequireFailure([&] {
                LoadObject(nlohmann::json::parse(
                    R"({"name":"BadScript","components":{"ScriptComp":{"script":"../escape.lua"}}})"));
            }, "ScriptComp should reject path traversal");

            ScriptEngine::EnsureApiStubs();
            Require(std::filesystem::is_regular_file(scriptDir / ".devilwood_api.lua") &&
                    std::filesystem::is_regular_file(scriptDir / ".luarc.json"),
                    "EnsureApiStubs did not generate VS Code Lua autocomplete files");
            const auto listedScripts = ScriptEngine::AvailableScripts();
            Require(std::find(listedScripts.begin(), listedScripts.end(), ".devilwood_api.lua") ==
                        listedScripts.end(),
                    "AvailableScripts should hide dot-prefixed API stub files");

            {
                std::ofstream out(scriptDir / "exposed.lua", std::ios::binary | std::ios::trunc);
                out << "properties = {\n"
                    << "    speed = 50.0,\n"
                    << "    enabled_move = true,\n"
                    << "    label = \"default\"\n"
                    << "}\n"
                    << "function on_start(self)\n"
                    << "    vars.set_global('lua_label', label)\n"
                    << "end\n"
                    << "function on_update(self, dt)\n"
                    << "    if enabled_move then\n"
                    << "        self:set_position(self:get_x() + speed * dt, self:get_y())\n"
                    << "    end\n"
                    << "end\n";
            }
            std::string inspectError;
            const auto inspected = ScriptEngine::InspectProperties("exposed.lua", &inspectError);
            Require(inspectError.empty() && inspected.size() == 3 &&
                    inspected[0].name == "enabled_move" &&
                    inspected[1].name == "label" &&
                    inspected[2].name == "speed",
                    "InspectProperties did not discover all declared script properties");

            const std::vector<VariableDefinition> propGlobals = {
                {"lua_label", "Lua Label", std::string{}}
            };
            game.scriptEngine.Reset();
            game.variables.ResetGlobals(propGlobals);
            auto propGraph = LoadSceneGraph({
                nlohmann::json::parse(R"({"name":"FastBox","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ScriptComp":{"script":"exposed.lua","properties":{"speed":200.0,"label":"fast"}}
                }})"),
                nlohmann::json::parse(R"({"name":"StoppedBox","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ScriptComp":{"script":"exposed.lua","properties":{"enabled_move":false,"label":"stopped"}}
                }})")
            }, true, &propGlobals, &luaSceneVars);
            Screen propScreen;
            for (auto& root : propGraph.roots)
                propScreen.addEntity(std::move(root));
            propScreen.SceneLoaded();
            propScreen.Update(0.5f);
            Require(std::abs(propScreen.entities[0]->getComponent<TransformComp>()->position.x - 100.0f) < 0.01f,
                    "Overridden numeric script property 'speed' was not applied");
            Require(std::abs(propScreen.entities[1]->getComponent<TransformComp>()->position.x - 0.0f) < 0.01f,
                    "Overridden boolean script property 'enabled_move' was not applied");
            Require(std::get<std::string>(game.variables.Get(VariableScope::Global, "lua_label")) == "stopped",
                    "Overridden string script property 'label' was not applied");

            {
                std::ofstream target(scriptDir / "target.lua", std::ios::binary | std::ios::trunc);
                target << "properties = { locked = true }\n"
                       << "function unlock(self, amount)\n"
                       << "    vars.set_global('lua_unlocked_by', self.name)\n"
                       << "    return amount * 2\n"
                       << "end\n";
                std::ofstream controller(scriptDir / "controller.lua", std::ios::binary | std::ios::trunc);
                controller << "function on_update(self, dt)\n"
                           << "    local t = self:get_child('Target')\n"
                           << "    local tr = t:get_component('Transform')\n"
                           << "    tr.x = 42\n"
                           << "    tr:translate(1, 2)\n"
                           << "    tr.rotation = 45\n"
                           << "    local rect = t:get_rectangle()\n"
                           << "    rect.width = 77\n"
                           << "    rect.opacity = 2\n"
                           << "    rect:set_color(10, 20, 30)\n"
                           << "    local _, _, b = rect:get_color()\n"
                           << "    local sc = t:get_script()\n"
                           << "    sc:set('locked', false)\n"
                           << "    local result = sc:call('unlock', 5)\n"
                           << "    local bonus = 0\n"
                           << "    if sc:get('locked') then bonus = bonus + 1000 end\n"
                           << "    if t:has_component('Light') then bonus = bonus + 5000 end\n"
                           << "    if t:get_component('Hotspot') ~= nil then bonus = bonus + 7000 end\n"
                           << "    vars.set_global('lua_result', result + b + bonus)\n"
                           << "end\n";
            }
            const std::vector<VariableDefinition> componentGlobals = {
                {"lua_result", "Lua Result", 0},
                {"lua_unlocked_by", "Lua Unlocked By", std::string{}}
            };
            game.scriptEngine.Reset();
            game.variables.ResetGlobals(componentGlobals);
            auto componentGraph = LoadSceneGraph({
                nlohmann::json::parse(R"({"name":"Controller","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ScriptComp":{"script":"controller.lua"}
                }})"),
                nlohmann::json::parse(R"({"name":"Target","parent":0,"components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "RectangleComp":{"width":10,"height":10},
                    "ScriptComp":{"script":"target.lua"}
                }})")
            }, true, &componentGlobals, &luaSceneVars);
            Screen componentScreen;
            for (auto& root : componentGraph.roots)
                componentScreen.addEntity(std::move(root));
            componentScreen.SceneLoaded();
            componentScreen.Update(0.0f);
            auto* targetEntity = componentScreen.entities[0]->getChild("Target");
            Require(targetEntity != nullptr, "Component API test target entity missing");
            const auto* targetTransform = targetEntity->getComponent<TransformComp>();
            const auto* targetRect = targetEntity->getComponent<RectangleComp>();
            Require(std::abs(targetTransform->position.x - 43.0f) < 0.01f &&
                        std::abs(targetTransform->position.y - 2.0f) < 0.01f &&
                        std::abs(targetTransform->rotation - 45.0f) < 0.01f,
                    "Lua TransformComponent properties were not applied");
            Require(std::abs(targetRect->width - 77.0f) < 0.01f && targetRect->opacity == 1.0f &&
                        targetRect->color.r == 10 && targetRect->color.b == 30,
                    "Lua RectangleComponent properties were not applied");
            Require(std::get<int>(game.variables.Get(VariableScope::Global, "lua_result")) == 40 &&
                        std::get<std::string>(game.variables.Get(VariableScope::Global, "lua_unlocked_by")) == "Target",
                    "Lua ScriptComponent get/set/call did not work across scripts");

            {
                std::ofstream refScript(scriptDir / "ref_mover.lua", std::ios::binary | std::ios::trunc);
                refScript << "properties = {\n"
                          << "    target_transform = ref(\"Transform\"),\n"
                          << "    target_rect = ref(\"Rectangle\"),\n"
                          << "    target_entity = ref(\"Entity\"),\n"
                          << "    unset_ref = ref(\"Light\")\n"
                          << "}\n"
                          << "function on_start(self)\n"
                          << "    if target_transform then target_transform.x = 123 end\n"
                          << "    if target_rect then target_rect.width = 88 end\n"
                          << "    if target_entity then vars.set_global('lua_ref_name', target_entity.name) end\n"
                          << "    if unset_ref ~= nil then vars.set_global('lua_ref_name', 'BAD') end\n"
                          << "end\n";
            }
            std::string refInspectError;
            const auto refProps = ScriptEngine::InspectProperties("ref_mover.lua", &refInspectError);
            Require(refInspectError.empty() && refProps.size() == 4 &&
                        refProps[0].name == "target_entity" && refProps[0].refType == "Entity" &&
                        refProps[1].name == "target_rect" && refProps[1].refType == "Rectangle" &&
                        refProps[2].name == "target_transform" && refProps[2].refType == "Transform" &&
                        refProps[3].name == "unset_ref" && refProps[3].refType == "Light",
                    "InspectProperties did not discover ref() object/component properties");

            const std::vector<VariableDefinition> refGlobals = {
                {"lua_ref_name", "Lua Ref Name", std::string{}}
            };
            game.scriptEngine.Reset();
            game.variables.ResetGlobals(refGlobals);
            auto refGraph = LoadSceneGraph({
                nlohmann::json::parse(R"({"name":"Driver","components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ScriptComp":{"script":"ref_mover.lua","properties":{
                        "target_transform":{"object":1},
                        "target_rect":{"object":1},
                        "target_entity":{"object":1}
                    }}
                }})"),
                nlohmann::json::parse(R"({"name":"DrivenBox","components":{
                    "TransformComp":{"position":{"x":5,"y":6},"scale":{"x":1,"y":1}},
                    "RectangleComp":{"width":12,"height":12}
                }})")
            }, true, &refGlobals, &luaSceneVars);
            Screen refScreen;
            for (auto& root : refGraph.roots)
                refScreen.addEntity(std::move(root));
            refScreen.SceneLoaded();
            Require(std::abs(refScreen.entities[1]->getComponent<TransformComp>()->position.x - 123.0f) < 0.01f,
                    "ref('Transform') did not inject target TransformComp into Lua");
            Require(std::abs(refScreen.entities[1]->getComponent<RectangleComp>()->width - 88.0f) < 0.01f,
                    "ref('Rectangle') did not inject target RectangleComp into Lua");
            Require(std::get<std::string>(game.variables.Get(VariableScope::Global, "lua_ref_name")) == "DrivenBox",
                    "ref('Entity') did not inject target Entity into Lua");

            // Verify SceneDocument::RemoveObject remaps ScriptComp object reference indices.
            auto refSceneDoc = SceneDocument::Create(managerDirectory, "script_ref_remap");
            refSceneDoc.AddObject(nlohmann::json::parse(R"({"name":"Dummy","components":{}})"));
            refSceneDoc.AddObject(nlohmann::json::parse(R"({"name":"Driver","components":{"ScriptComp":{
                "script":"ref_mover.lua",
                "properties":{"target_transform":{"object":2},"deleted_ref":{"object":0}}
            }}})"));
            refSceneDoc.AddObject(nlohmann::json::parse(R"({"name":"Target","components":{}})"));
            refSceneDoc.RemoveObject(0);
            const auto remappedProps = refSceneDoc.Object(0)["components"]["ScriptComp"]["properties"];
            Require(remappedProps["target_transform"]["object"].get<std::size_t>() == 1 &&
                        remappedProps["deleted_ref"]["object"].is_null(),
                    "RemoveObject did not remap ScriptComp object reference indices");

            // Verify EnableLens / DisableLens / ToggleLens UI interaction actions.
            game.SetLensEnabled(false);
            auto lensUiDoc = SceneDocument::Create(managerDirectory, "lens_ui_test");
            lensUiDoc.AddObject(nlohmann::json::parse(R"({
                "id":"btn_enable_lens","name":"EnableLensBtn","visible":true,"enabled":true,
                "components":{
                    "TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "InteractionComp":{"actions":[{"type":"EnableLens"}]}
                }
            })"));
            lensUiDoc.AddObject(nlohmann::json::parse(R"({
                "id":"btn_disable_lens","name":"DisableLensBtn","visible":true,"enabled":true,
                "components":{
                    "TransformComp":{"position":{"x":50,"y":0},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "InteractionComp":{"actions":[{"type":"DisableLens"}]}
                }
            })"));
            lensUiDoc.AddObject(nlohmann::json::parse(R"({
                "id":"btn_toggle_lens","name":"ToggleLensBtn","visible":true,"enabled":true,
                "components":{
                    "TransformComp":{"position":{"x":100,"y":0},"scale":{"x":1,"y":1}},
                    "ColliderComp":{"width":40,"height":40},
                    "InteractionComp":{"actions":[{"type":"ToggleLens"}]}
                }
            })"));
            auto lensUi = ProjectUI::FromDocument(lensUiDoc);
            Require(lensUi->TriggerAt({10, 10}), "EnableLens UI button did not trigger");
            lensUi->Update(0.0f);
            Require(game.IsLensEnabled() && Global::lensOn, "EnableLens action did not enable lens");
            Require(lensUi->TriggerAt({60, 10}), "DisableLens UI button did not trigger");
            lensUi->Update(0.0f);
            Require(!game.IsLensEnabled() && !Global::lensOn, "DisableLens action did not disable lens");
            Require(lensUi->TriggerAt({110, 10}), "ToggleLens UI button did not trigger");
            lensUi->Update(0.0f);
            Require(game.IsLensEnabled() && Global::lensOn, "ToggleLens action did not toggle lens on");
            Require(lensUi->TriggerAt({110, 10}), "Second ToggleLens UI button click did not trigger");
            lensUi->Update(0.0f);
            Require(!game.IsLensEnabled() && !Global::lensOn, "ToggleLens action did not toggle lens off");
        }
        {
            const auto saveDir = managerDirectory / "save_system_test";
            std::filesystem::create_directories(saveDir);
            const std::vector<VariableDefinition> saveGlobals = {
                {"progress", "Progress", 0},
                {"unlocked", "Unlocked", false}
            };
            VariableStore::SaveGlobalDefinitions(saveDir, saveGlobals);

            auto sceneA = SceneDocument::Create(saveDir, "scene_a");
            sceneA.SetVariables({{"local_step", "Local Step", 0}});
            sceneA.Save();
            auto sceneB = SceneDocument::Create(saveDir, "scene_b");
            sceneB.SetVariables({{"b_flag", "B Flag", false}});
            sceneB.Save();

            SceneManager saveManager(saveDir);
            saveManager.RegisterScene(sceneA.Path());
            saveManager.RegisterScene(sceneB.Path());

            game.scene_manager = std::make_unique<SceneManager>(saveDir);
            game.itemDB = itemDatabase;
            game.variables.ResetGlobals(saveGlobals);
            game.variables.ResetScene({{"local_step", "Local Step", 0}});
            game.item_tracker.Reset();
            game.runtime_inv.assign(5, Item());
            Global::selectedItemId = 0;

            game.variables.Set(VariableScope::Global, "progress", 7);
            game.variables.Set(VariableScope::Global, "unlocked", true);
            game.variables.Set(VariableScope::Scene, "local_step", 3);
            game.AddItem(-42);

            Require(!game.HasSaveGame(1), "Save slot 1 should not exist before saving");
            Require(game.SaveGame(1), "Game::SaveGame(1) failed");
            Require(game.HasSaveGame(1), "Game::HasSaveGame(1) should be true after saving");

            // Mutate all runtime state
            game.variables.Set(VariableScope::Global, "progress", 0);
            game.variables.Set(VariableScope::Global, "unlocked", false);
            game.variables.Set(VariableScope::Scene, "local_step", 0);
            game.removeItem(-42);
            game.item_tracker.Reset();

            // Load slot 1 and verify everything is restored
            Require(game.LoadGame(1), "Game::LoadGame(1) failed");
            Require(std::get<int>(game.variables.Get(VariableScope::Global, "progress")) == 7 &&
                    std::get<bool>(game.variables.Get(VariableScope::Global, "unlocked")),
                    "Global variables were not restored on LoadGame");
            Require(std::get<int>(game.variables.Get(VariableScope::Scene, "local_step")) == 3,
                    "Scene variables were not restored on LoadGame");
            Require(game.hasItem(-42) && game.item_tracker.hasItem(-42),
                    "Inventory and picked item tracker were not restored on LoadGame");

            // Test SaveGame / DeleteSave interaction actions
            auto saveActionObj = LoadObject(nlohmann::json::parse(
                R"({"name":"SaveAct","components":{"InteractionComp":{"actions":[
                    {"type":"SaveGame","slot":2},{"type":"DeleteSave","slot":1}
                ]}}})"));
            saveActionObj.entity->getComponent<InteractionComp>()->Trigger();
            saveActionObj.entity->getComponent<InteractionComp>()->Update(0.0f);
            Require(game.HasSaveGame(2) && !game.HasSaveGame(1),
                    "SaveGame / DeleteSave interaction actions did not execute");
            Require(game.DeleteSaveGame(2) && !game.HasSaveGame(2),
                    "DeleteSaveGame(2) did not remove save file");

            game.scene_stak.reset();
            game.scene_manager.reset();
            game.item_tracker.Reset();
            game.runtime_inv.assign(3, Item());
            Global::selectedItemId = 0;
        }
        RequireFailure([&] { projectUI->SetVisible("missing", true); },
                       "Missing project UI target was accepted");
        auto restartedUI = ProjectUI::Load(managerDirectory);
        Require(!restartedUI->IsVisible("status") && restartedUI->IsEnabled("status"),
                "Project UI did not restore its saved state on reload");
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"status","name":"A","components":{}},{"id":"status","name":"B","components":{}}]})";
        }
        RequireFailure([&] { ProjectUI::Load(managerDirectory); }, "Duplicate UI IDs were accepted");
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[
                {"id":"child","name":"Child","parent":1,"alpha":0.5,"components":{"TransformComp":{"position":{"x":5,"y":3},"scale":{"x":1,"y":1}},"ColliderComp":{"width":10,"height":10}}},
                {"id":"group","name":"Inventory","group":true,"alpha":0.8,"components":{"TransformComp":{"position":{"x":20,"y":10},"scale":{"x":2,"y":2}}}},
                {"id":"other","name":"Crafting","group":true,"components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}}}},
                {"id":"nested","name":"Nested","parent":0,"alpha":0.5,"components":{}}
            ]})";
        }
        auto groups = ProjectUI::Load(managerDirectory);
        const auto childPosition = groups->GetElement("child").getComponent<TransformComp>()->getWorldPosition();
        Require(childPosition.x == 30 && childPosition.y == 16 &&
                std::abs(groups->EffectiveAlpha("child") - 0.4f) < 0.0001f &&
                std::abs(groups->EffectiveAlpha("nested") - 0.2f) < 0.0001f,
                "UI hierarchy did not inherit transform and alpha");
        auto uiDocument = SceneDocument::Open(uiPath);
        ReparentPreservingWorld(uiDocument, 0, 2);
        auto reparentedUI = ProjectUI::FromDocument(uiDocument);
        const auto reparentedPosition = reparentedUI->GetElement("child").getComponent<TransformComp>()->getWorldPosition();
        Require(reparentedPosition.x == childPosition.x && reparentedPosition.y == childPosition.y &&
                reparentedUI->GetElement("child").parent == &reparentedUI->GetElement("other"),
                "Reparenting UI did not preserve its world position");
        int groupChildUpdates = 0, groupChildDraws = 0;
        groups->GetElement("child").addComponent<UIStateCounter>(groupChildUpdates, groupChildDraws);
        Global::mousePos = {31, 17};
        Require(groups->IsUnderMouse(), "Enabled UI child did not receive input");
        groups->SetVisible("group", false);
        groups->Update(0);
        groups->Draw();
        Require(groupChildUpdates == 1 && groupChildDraws == 0 && !groups->IsUnderMouse(),
                "Hidden parent did not hide descendants and block input");
        groups->SetVisible("group", true);
        groups->SetEnabled("group", false);
        groups->Update(0);
        groups->Draw();
        Require(groupChildUpdates == 1 && groupChildDraws == 1 && !groups->IsUnderMouse() &&
                groups->IsEnabled("child") && groups->IsEnabled("other"),
                "Disabling a group did not suppress descendants independently of other groups");
        groups->FadeGroup("group", false, 1.0f);
        groups->Update(0.5f);
        Require(groups->IsFading("group") && groups->IsVisible("group") &&
                std::abs(groups->EffectiveAlpha("child") - 0.2f) < 0.0001f,
                "Group fade did not progress while disabled");
        groups->Update(0.5f);
        Require(!groups->IsFading("group") && !groups->IsVisible("group") &&
                groups->Alpha("group") == 0.0f,
                "Fade-out did not hide the group on completion");
        groups->FadeGroup("group", true, 1.0f);
        groups->Update(1.0f);
        Require(groups->IsVisible("group") && !groups->IsFading("group") &&
                std::abs(groups->Alpha("group") - 0.8f) < 0.0001f,
                "Fade-in did not restore the authored alpha");
        groups->FadeGroup("group", false, 0);
        groups->SetVisible("group", true);
        Require(groups->IsVisible("group") && std::abs(groups->Alpha("group") - 0.8f) < 0.0001f,
                "Showing a faded-out group did not restore its alpha");
        {
            auto interactiveDocument = SceneDocument::Create(managerDirectory, "interactive_ui");
            interactiveDocument.AddObject();
            interactiveDocument.AddObject();
            interactiveDocument.SetObject(0, {
                {"id", "interactive-group"}, {"name", "Interactive group"}, {"group", true},
                {"components", {
                    {"TransformComp", {{"position", {{"x", 20}, {"y", 10}}},
                                       {"scale", {{"x", 1}, {"y", 1}}}}}
                }}
            });
            interactiveDocument.SetObject(1, {
                {"id", "button"}, {"name", "Button"}, {"parent", 0},
                {"components", {
                    {"TransformComp", {{"position", {{"x", 0}, {"y", 0}}},
                                       {"scale", {{"x", 1}, {"y", 1}}}}},
                    {"ColliderComp", {{"width", 40}, {"height", 30}}},
                    {"InteractionComp", {{"actions", {{{"type", "DisableUI"},
                                                       {"ui", "interactive-group"}}}}}}
                }}
            });
            auto interactiveUI = ProjectUI::FromDocument(interactiveDocument);
            UI_Manager interactiveManager;
            game.ui_m = &interactiveManager;
            interactiveManager.projectUI = std::move(interactiveUI);
            Require(interactiveManager.projectUI->TriggerAt({25, 15}),
                    "Visible project UI element did not trigger its interaction");
            interactiveManager.projectUI->Update(0);
            Require(!interactiveManager.projectUI->IsEnabled("interactive-group") &&
                    interactiveManager.projectUI->IsEnabled("button"),
                    "UI interaction did not disable the group with inherited child behavior");
            Require(!interactiveManager.projectUI->TriggerAt({25, 15}),
                    "Disabled UI group child still received interaction input");
            game.ui_m = nullptr;
        }
        UI_Manager groupManager;
        game.ui_m = &groupManager;
        groupManager.projectUI = ProjectUI::Load(managerDirectory);
        auto fadeInteraction = LoadObject(nlohmann::json::parse(R"({"name":"Fade","components":{"InteractionComp":{"actions":[
            {"type":"FadeOutUI","ui":"group","duration":1},
            {"type":"DisableUI","ui":"group"},
            {"type":"EnableUI","ui":"other"}
        ]}}})"));
        auto* fadeActions = fadeInteraction.entity->getComponent<InteractionComp>();
        fadeActions->Trigger();
        fadeActions->Update(0);
        groupManager.projectUI->Update(0.5f);
        fadeActions->Update(0);
        Require(fadeActions->IsRunning() && groupManager.projectUI->IsEnabled("group"),
                "Interaction did not wait for the UI fade");
        groupManager.projectUI->Update(0.5f);
        fadeActions->Update(0);
        Require(!groupManager.projectUI->IsEnabled("group") &&
                !groupManager.projectUI->IsVisible("group"),
                "Action after UI fade did not run on completion");
        fadeActions->Update(0);
        fadeActions->Update(0);
        Require(!fadeActions->IsRunning(), "UI action list did not finish");
        auto toggleInteraction = LoadObject(nlohmann::json::parse(R"({"name":"Toggle","components":{"InteractionComp":{"actions":[
            {"type":"ToggleUI","ui":"group"}
        ]}}})"));
        auto* toggleActions = toggleInteraction.entity->getComponent<InteractionComp>();
        toggleActions->Trigger();
        toggleActions->Update(0);
        Require(groupManager.projectUI->IsVisible("group") &&
                groupManager.projectUI->IsEnabled("group") &&
                !toggleActions->IsRunning(),
                "Toggle UI action did not immediately show and enable hidden UI");
        groupManager.projectUI->SetVisible("group", false);
        groupManager.projectUI->SetEnabled("group", false);
        auto showAndFadeIn = LoadObject(nlohmann::json::parse(R"({"name":"ShowFadeIn","components":{"InteractionComp":{"actions":[
            {"type":"FadeInUI","ui":"group","duration":0.5}
        ]}}})"));
        auto* fadeInActions = showAndFadeIn.entity->getComponent<InteractionComp>();
        fadeInActions->Trigger();
        fadeInActions->Update(0);
        Require(groupManager.projectUI->IsVisible("group") &&
                groupManager.projectUI->IsEnabled("group"),
                "FadeInUI did not show and enable disabled hidden UI");
        groupManager.projectUI->Update(0.5f);
        fadeInActions->Update(0);
        groupManager.projectUI->SetVisible("other", false);
        groupManager.projectUI->SetEnabled("other", false);
        auto showOnly = LoadObject(nlohmann::json::parse(R"({"name":"ShowOnly","components":{"InteractionComp":{"actions":[
            {"type":"ShowUI","ui":"other"}
        ]}}})"));
        showOnly.entity->getComponent<InteractionComp>()->Trigger();
        showOnly.entity->getComponent<InteractionComp>()->Update(0);
        Require(groupManager.projectUI->IsVisible("other") &&
                groupManager.projectUI->IsEnabled("other"),
                "ShowUI did not show and enable disabled hidden UI");
        auto hideOnly = LoadObject(nlohmann::json::parse(R"({"name":"HideOnly","components":{"InteractionComp":{"actions":[
            {"type":"HideUI","ui":"other"}
        ]}}})"));
        hideOnly.entity->getComponent<InteractionComp>()->Trigger();
        hideOnly.entity->getComponent<InteractionComp>()->Update(0);
        Require(!groupManager.projectUI->IsVisible("other") &&
                !groupManager.projectUI->IsEnabled("other"),
                "HideUI did not hide and disable UI");
        Global::mousePos = {20, 20};
        groupManager.projectUI->SetEnabled("other", true);
        Require(!groupManager.projectUI->IsUnderMouse(),
                "Hidden UI element should never report IsUnderMouse()");

        game.SetLensOffset({15.0f, -25.0f});
        Global::mousePos = {100.0f, 80.0f};
        game.SetLensEnabled(true);
        Require(std::abs(Global::lensPosition.x - 115.0f) < 0.01f &&
                std::abs(Global::lensPosition.y - 55.0f) < 0.01f,
                "Lens position did not align with mousePos + lensOffset");
        game.SetLensEnabled(false);
        game.SetLensOffset({0.0f, 0.0f});

        {
            const auto lensSpriteObj = LoadObject(nlohmann::json::parse(R"({
                "name":"HiddenClue",
                "components":{
                    "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                    "Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":16,"height":16},"lens_only":true}
                }
            })"));
            const auto* sprite = lensSpriteObj.entity->getComponent<Sprite>();
            Require(sprite && sprite->lensOnly, "Sprite lens_only=true was not loaded");
            const auto normalSpriteObj = LoadObject(nlohmann::json::parse(R"({
                "name":"NormalSprite",
                "components":{
                    "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                    "Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":16,"height":16}}
                }
            })"));
            const auto* normalSprite = normalSpriteObj.entity->getComponent<Sprite>();
            Require(normalSprite && !normalSprite->lensOnly, "Sprite lens_only should default to false");
            RequireFailure([&] {
                LoadObject(nlohmann::json::parse(R"({
                    "name":"BadLensSprite",
                    "components":{
                        "TransformComp":{"position":{"x":10,"y":20},"scale":{"x":1,"y":1}},
                        "Sprite":{"atlas":"atlas_1.png","source":{"x":0,"y":0,"width":16,"height":16},"lens_only":"yes"}
                    }
                })"));
            }, "Sprite lens_only should reject non-boolean values");
        }
        toggleActions->Trigger();
        toggleActions->Update(0);
        Require(!groupManager.projectUI->IsVisible("group"),
                "Toggle UI action did not immediately hide visible UI");

        auto toggleFadeInteraction = LoadObject(nlohmann::json::parse(R"({"name":"Toggle fade","components":{"InteractionComp":{"actions":[
            {"type":"ToggleFadeUI","ui":"group","duration":1}
        ]}}})"));
        auto* toggleFadeActions = toggleFadeInteraction.entity->getComponent<InteractionComp>();
        toggleFadeActions->Trigger();
        toggleFadeActions->Update(0);
        Require(groupManager.projectUI->IsVisible("group") &&
                groupManager.projectUI->IsFading("group") &&
                groupManager.projectUI->IsFadingIn("group"),
                "Toggle fade did not fade hidden UI in");
        groupManager.projectUI->Update(1);
        toggleFadeActions->Update(0);
        toggleFadeActions->Trigger();
        toggleFadeActions->Update(0);
        Require(groupManager.projectUI->IsFading("group") &&
                !groupManager.projectUI->IsFadingIn("group"),
                "Toggle fade did not fade visible UI out");
        groupManager.projectUI->Update(1);
        toggleFadeActions->Update(0);
        Require(!groupManager.projectUI->IsVisible("group"),
                "Toggle fade did not hide UI after fade-out");
        game.ui_m = nullptr;
        auto badFade = nlohmann::json::parse(R"({"name":"Bad fade","components":{"InteractionComp":{"actions":[{"type":"FadeInUI","ui":"group","duration":-1}]}}})");
        RequireFailure([&] { LoadObject(badFade); }, "Negative UI fade duration was accepted");
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"a","name":"A","group":true,"parent":1,"components":{}},{"id":"b","name":"B","group":true,"components":{}}]})";
        }
        RequireFailure([&] { ProjectUI::Load(managerDirectory); }, "Nested UI group was accepted");
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"a","name":"A","alpha":1.1,"components":{}}]})";
        }
        RequireFailure([&] { ProjectUI::Load(managerDirectory); }, "Invalid UI alpha was accepted");
        {
            std::ofstream output(uiPath);
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"status","name":"Status","visible":false,"enabled":true,"components":{}}]})";
        }
        UI_Manager uiManager;
        game.ui_m = &uiManager;
        uiManager.projectUI = ProjectUI::Load(managerDirectory);
        auto uiInteraction = LoadObject(nlohmann::json::parse(R"({"name":"UI interaction","components":{"InteractionComp":{"actions":[{"type":"ShowUI","ui":"status"},{"type":"HideUI","ui":"status"}]}}})"));
        auto* uiActions = uiInteraction.entity->getComponent<InteractionComp>();
        uiActions->Trigger();
        Require(uiActions->IsRunning(), "UI action did not start");
        uiActions->Update(0);
        Require(!uiManager.projectUI->IsVisible("status"), "UI actions did not execute in order");
        uiActions->Update(0);
        Require(!uiActions->IsRunning(), "UI action list did not finish");
        game.ui_m = nullptr;
        uiActions->Trigger();
        Require(!uiActions->IsRunning(), "UI action ran without a project UI manager");
        auto invalidUiAction = nlohmann::json::parse(R"([{"name":"Interaction","components":{"InteractionComp":{"actions":[{"type":"ShowUI","ui":""}]}}}])")
            .get<std::vector<nlohmann::json>>();
        RequireFailure([&] { LoadSceneGraph(invalidUiAction, true); },
                       "An unconfigured UI action was accepted at runtime");
        auto subtitleActionScene = nlohmann::json::parse(
            R"([{"name":"Subtitle interaction","components":{"InteractionComp":{"actions":[{"type":"ShowSubtitle","text":"A message"}]}}}])")
            .get<std::vector<nlohmann::json>>();
        auto subtitleActionGraph = LoadSceneGraph(subtitleActionScene, true);
        Require(subtitleActionGraph.objects[0]
                    ->getComponent<InteractionComp>()->actions[0].type ==
                    InteractionComp::ActionData::Type::ShowSubtitle,
                "A ShowSubtitle action prevented the runtime scene from loading");
        Require(manager.RegisteredScenes().empty(), "New scene manager should have no registered scenes");
        manager.RegisterScene(runtimeScenePath);
        Require(manager.RegisteredScenes().size() == 1, "Scene was not registered with the game");
        manager.SetStartupScene(runtimeScenePath);
        Require(manager.StartupScene() == runtimeScenePath, "Startup scene was not saved");
        Require(!manager.StartupFadeIn(), "Startup fade-in should default to disabled");
        manager.SetStartupFadeIn(true);
        Require(manager.StartupFadeIn(), "Startup fade-in option was not saved");
        SceneManager reloadedManager(managerDirectory);
        Require(reloadedManager.StartupFadeIn(),
                "Startup fade-in option was not persisted in the catalog");
        auto runtimeScreen = manager.LoadScene(runtimeScenePath);
        Require(runtimeScreen && runtimeScreen->entities.size() == 3 &&
                runtimeScreen->BaseDarkness() == 0.3f &&
                runtimeScreen->entities[0]->getComponent<LightComp>() != nullptr &&
                runtimeScreen->entities[1]->IsUiLayer() &&
                runtimeScreen->entities[1]->getComponent<TextComp>() != nullptr &&
                runtimeScreen->entities[2]->getComponent<RectangleComp>() != nullptr,
                "Registered JSON scene did not load its point light, UI, and rectangle");
        const auto hotspotPath = managerDirectory / "hotspot.json";
        {
            std::ofstream output(hotspotPath);
            output << R"({"version":1,"name":"Hotspot test","objects":[{"name":"Hotspot","components":{"TransformComp":{"position":{"x":20,"y":30},"scale":{"x":1,"y":1}},"ColliderComp":{"width":40,"height":30},"HotspotComp":{"interaction":1}}},{"name":"Interaction","components":{"InteractionComp":{"actions":[{"type":"ChangeScene","scene":"runtime.json"}]}}}]})";
            if (!output)
                throw std::runtime_error("Unable to create hotspot test scene");
        }
        manager.RegisterScene(hotspotPath);
        auto hotspotScreen = manager.LoadScene(hotspotPath);
        auto malformed = nlohmann::json::parse(R"([
            {"name":"Hotspot","components":{"TransformComp":{"position":{"x":0,"y":0},"scale":{"x":1,"y":1}},"ColliderComp":{"width":10,"height":10},"HotspotComp":{"interaction":4}}},
            {"name":"Interaction","components":{"InteractionComp":{"actions":[{"type":"ChangeScene","scene":"runtime.json"}]}}}
        ])").get<std::vector<nlohmann::json>>();
        RequireFailure([&] { LoadSceneGraph(malformed, true); },
                       "Invalid hotspot interaction index was accepted");
        malformed[0]["components"]["HotspotComp"]["interaction"] = 1;
        malformed[1]["components"]["InteractionComp"]["actions"][0]["scene"] = "../outside.json";
        RequireFailure([&] { LoadSceneGraph(malformed, true); },
                       "Unsafe action scene path was accepted");
        malformed[1]["components"]["InteractionComp"]["actions"][0]["scene"] = "";
        RequireFailure([&] { LoadSceneGraph(malformed, true); },
                       "Unconfigured Change Scene action was accepted at runtime");
        auto rotated = LoadObject(nlohmann::json::parse(R"({"name":"Rotated","components":{"TransformComp":{"position":{"x":100,"y":100},"scale":{"x":2,"y":1},"rotation":90},"ColliderComp":{"width":20,"height":10},"HotspotComp":{"interaction":null}}})"));
        auto* rotatedHotspot = rotated.entity->getComponent<HotspotComp>();
        Require(rotatedHotspot->Contains({95, 110}) && !rotatedHotspot->Contains({110, 110}),
                "Hotspot hit testing did not follow its rotated transform");
        auto parented = LoadSceneGraph(nlohmann::json::parse(R"([
            {"name":"Parent","components":{"TransformComp":{"position":{"x":100,"y":50},"scale":{"x":2,"y":2},"rotation":90}}},
            {"name":"Hotspot","parent":0,"components":{"TransformComp":{"position":{"x":10,"y":0},"scale":{"x":1,"y":1}},"ColliderComp":{"width":10,"height":5},"HotspotComp":{"interaction":null}}}
        ])").get<std::vector<nlohmann::json>>(), true);
        Require(parented.objects[1]->getComponent<HotspotComp>()->Contains({96, 75}),
                "Parented hotspot did not inherit its parent's transform");
        auto* hotspot = hotspotScreen->entities[0]->getComponent<HotspotComp>();
        auto* interaction = hotspotScreen->entities[1]->getComponent<InteractionComp>();
        Require(hotspot && interaction && hotspot->interaction == interaction,
                "Hotspot was not linked to its interaction");
        game.scene_manager = std::make_unique<SceneManager>(managerDirectory);
        hotspotScreen->entities[0]->setActive(false);
        Require(!hotspotScreen->TriggerHotspotAt({25, 35}),
                "Inactive hotspot received a click");
        hotspotScreen->entities[0]->setActive(true);
        Require(!hotspotScreen->TriggerHotspotAt({1, 1}) && !interaction->IsRunning(),
                "Click outside hotspot started an interaction");
        Require(hotspotScreen->TriggerHotspotAt({25, 35}) && interaction->IsRunning(),
                "Click inside hotspot did not start its interaction");
        interaction->Update(0);
        Require(game.fm.fadeout, "Change Scene action did not start its transition");
        game.fm.elapsed = 0.2f;
        hotspotScreen->TriggerHotspotAt({25, 35});
        interaction->Update(0);
        Require(game.fm.elapsed == 0.2f, "Repeated click restarted the running interaction");
        game.fm.fadeout = false;
        game.fm.onFadeComplete = nullptr;
        interaction->Update(0);
        Require(!interaction->IsRunning(), "Interaction did not finish");
        hotspotScreen->TriggerHotspotAt({25, 35});
        Require(interaction->IsRunning(), "Finished interaction could not be started again");
        game.scene_manager.reset();
        manager.UnregisterScene(hotspotPath);
        RequireFailure([&] { manager.RegisterScene(tooManyLightsPath); },
                       "Runtime scene exceeded the shader light limit");
        RequireFailure([&] { manager.RegisterScene(unsupportedScenePath); },
                       "Unsupported runtime component was registered");
        Require(manager.RegisteredScenes().size() == 1,
                "Invalid scene changed the runtime scene catalog");
        manager.UnregisterScene(runtimeScenePath);
        Require(manager.RegisteredScenes().empty() && !manager.StartupScene() &&
                !manager.StartupFadeIn(),
                "Removing a game scene did not clear its startup selection");

        std::filesystem::remove_all(managerDirectory);
        std::cout << "Scene object tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::error_code ec;
        std::filesystem::remove_all(managerDirectory, ec);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
