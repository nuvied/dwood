#pragma once

#include <filesystem>
#include <string>

class ProjectConfig
{
public:
    static constexpr const char* FileName = "devilwood.project.json";

    std::filesystem::path root;
    std::string name;
    std::filesystem::path scenes = "scenes";
    std::filesystem::path resources = "resource";
    std::filesystem::path shaders = "shaders";

    static ProjectConfig Create(const std::filesystem::path& directory,
                                const std::string& projectName,
                                const std::filesystem::path& resourceTemplate,
                                const std::filesystem::path& shaderTemplate);
    static ProjectConfig Load(const std::filesystem::path& path);
    std::filesystem::path FilePath() const;
    std::filesystem::path ScenesPath() const;
    std::filesystem::path ResourcesPath() const;
    std::filesystem::path ShadersPath() const;
    void Save() const;
};

namespace AssetPaths
{
void Set(const std::filesystem::path& resources, const std::filesystem::path& shaders);
std::filesystem::path Resource(const std::filesystem::path& relativePath);
std::filesystem::path Shader(const std::filesystem::path& relativePath);
}
