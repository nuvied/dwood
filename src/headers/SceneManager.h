#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Screen;

class SceneManager
{
public:
    explicit SceneManager(std::filesystem::path directory);
    const std::filesystem::path& Directory() const { return directory; }

    std::vector<std::filesystem::path> RegisteredScenes() const;
    std::optional<std::filesystem::path> StartupScene() const;
    bool StartupFadeIn() const;
    void RegisterScene(const std::filesystem::path& scenePath);
    void UnregisterScene(const std::filesystem::path& scenePath);
    void SetStartupScene(const std::filesystem::path& scenePath);
    void SetStartupFadeIn(bool enabled);
    std::unique_ptr<Screen> LoadScene(const std::filesystem::path& scenePath) const;

private:
    struct Catalog
    {
        std::vector<std::string> scenes;
        std::optional<std::string> startup;
        bool startupFadeIn = false;
    };

    std::filesystem::path CatalogPath() const;
    std::string SceneKey(const std::filesystem::path& scenePath) const;
    Catalog ReadCatalog() const;
    void WriteCatalog(const Catalog& catalog) const;

    std::filesystem::path directory;
};
