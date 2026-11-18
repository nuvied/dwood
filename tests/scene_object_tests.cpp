#include "SceneObject.h"
#include "SceneDocument.h"
#include "SceneManager.h"
#include "ResourcesLoader.h"
#include "Game.hpp"
#include "Screen.h"
#include "ProjectUI.h"
#include "UI_Manager.h"

#include <chrono>
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
Music ResourcesLoader::bg_music;

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
        std::filesystem::create_directories(directory / ".devilwood");
        {
            std::ofstream output(ProjectUI::Path(directory));
            output << R"({"version":1,"name":"Project UI","objects":[{"id":"status","name":"Status","visible":false,"components":{}}]})";
            Require(static_cast<bool>(output), "Unable to write graphics UI fixture");
        }
        UI_Manager uiManager;
        game.ui_m = &uiManager;
        uiManager.projectUI = ProjectUI::Load(directory);
        ResourcesLoader::lenseShader = LoadShader(nullptr, "shaders/LenseShader.fs");
        game.scene_manager = std::make_unique<SceneManager>(directory);
        game.scene_manager->RegisterScene(targetPath);
        game.scene_manager->RegisterScene(sourcePath);
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
    if (argc > 1 && std::string(argv[1]) == "--interaction-graphics")
        return TestInteractionGraphics();
    auto& game = Game::get_Instance();
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
        auto data = nlohmann::json::parse(R"({"name":"Door","components":{"TransformComp":{"position":{"x":12,"y":23},"scale":{"x":0.5,"y":2},"rotation":30},"Sprite":{"atlas":"atlas_1.png","source":{"x":10,"y":20,"width":30,"height":40}}}})");
        auto object = LoadObject(data);
        auto* transform = object.entity->getComponent<TransformComp>();
        auto* sprite = object.entity->getComponent<Sprite>();
        Require(object.entity->name == "Door" && transform && sprite, "Game components were not created");
        Require(transform->position.x == 12 && transform->position.y == 23 &&
                transform->scale.x == 0.5f && transform->scale.y == 2 && transform->rotation == 30,
                "Transform component values are incorrect");
        Require(sprite->texture.id == 1 && sprite->src_rec.x == 10 && sprite->src_rec.y == 20 &&
                sprite->src_rec.width == 30 && sprite->src_rec.height == 40 && SourceFits(*sprite),
                "Sprite component values are incorrect");

        data["components"].erase("TransformComp");
        auto spriteOnly = LoadObject(data);
        Require(!spriteOnly.entity->getComponent<TransformComp>() && spriteOnly.entity->getComponent<Sprite>(),
                "Sprite-only object did not load");
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
            output << R"({"version":1,"name":"Runtime","objects":[{"name":"Point light","components":{"LightComp":{"intensity":1.5,"radius":80,"color":{"r":255,"g":200,"b":100,"a":255}}}},{"name":"UI","ui":true,"components":{"TextComp":{"text":"Hello","size":12}}}]})";
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
        SceneManager manager(managerDirectory);
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
        Require(groupChildUpdates == 1 && groupChildDraws == 0 && groups->IsUnderMouse(),
                "Hidden parent did not hide descendants while preserving updates and input");
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
                !toggleActions->IsRunning(),
                "Toggle UI action did not immediately show hidden UI");
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
        Require(runtimeScreen && runtimeScreen->entities.size() == 2 &&
                runtimeScreen->entities[0]->getComponent<LightComp>() != nullptr &&
                runtimeScreen->entities[1]->IsUiLayer() &&
                runtimeScreen->entities[1]->getComponent<TextComp>() != nullptr,
                "Registered JSON scene did not load its point light and UI");
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
