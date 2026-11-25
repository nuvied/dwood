#include "ProjectConfig.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace
{
std::filesystem::path NormalizeDirectory(const std::filesystem::path& root,
                                         const std::filesystem::path& relative,
                                         const char* label)
{
    if (relative.empty() || relative.is_absolute())
        throw std::runtime_error(std::string("Project ") + label + " path must be relative.");
    for (const auto& part : relative)
        if (part == "..")
            throw std::runtime_error(std::string("Project ") + label + " path cannot escape the project folder.");
    return (root / relative).lexically_normal();
}

void CopyTemplate(const std::filesystem::path& source, const std::filesystem::path& target,
                  const char* label)
{
    if (!std::filesystem::is_directory(source))
        throw std::runtime_error(std::string("Built-in ") + label + " template is missing: " + source.string());
    std::filesystem::create_directories(target);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
        const auto relative = std::filesystem::relative(entry.path(), source);
        const auto destination = target / relative;
        if (entry.is_directory())
            std::filesystem::create_directories(destination);
        else if (entry.is_regular_file()) {
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(entry.path(), destination,
                std::filesystem::copy_options::skip_existing);
        }
    }
}
}

ProjectConfig ProjectConfig::Create(const std::filesystem::path& directory,
                                    const std::string& projectName,
                                    const std::filesystem::path& resourceTemplate,
                                    const std::filesystem::path& shaderTemplate)
{
    if (projectName.empty() || projectName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-") != std::string::npos)
        throw std::runtime_error("Project name may contain letters, numbers, spaces, underscores, and hyphens only.");
    if (directory.empty())
        throw std::runtime_error("Choose a project directory.");

    ProjectConfig config;
    config.root = std::filesystem::absolute(directory).lexically_normal();
    config.name = projectName;
    if (std::filesystem::exists(config.FilePath()))
        throw std::runtime_error("A Devilwood project already exists in this directory.");

    std::filesystem::create_directories(config.ScenesPath() / ".devilwood");
    std::filesystem::create_directories(config.ScriptsPath());
    CopyTemplate(resourceTemplate, config.ResourcesPath(), "resource");
    CopyTemplate(shaderTemplate, config.ShadersPath(), "shader");
    config.Save();
    return config;
}

ProjectConfig ProjectConfig::Load(const std::filesystem::path& inputPath)
{
    ProjectConfig config;
    const auto candidate = std::filesystem::absolute(inputPath).lexically_normal();
    const auto file = std::filesystem::is_directory(candidate)
        ? candidate / FileName : candidate;
    config.root = file.parent_path();
    if (!std::filesystem::exists(file)) {
        config.name = config.root.filename().string();
        if (config.name.empty())
            config.name = "Devilwood Project";
        return config;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open project file: " + file.string());
    nlohmann::json data;
    try {
        input >> data;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid project file: " + std::string(e.what()));
    }
    if (!data.is_object() || data.value("version", 0) != 1 ||
        !data.contains("name") || !data["name"].is_string())
        throw std::runtime_error("Project file must contain version 1 and a name.");
    config.name = data["name"].get<std::string>();
    config.scenes = data.value("scenes", std::string("scenes"));
    config.resources = data.value("resources", std::string("resource"));
    config.shaders = data.value("shaders", std::string("shaders"));
    config.scripts = data.value("scripts", std::string("scripts"));
    config.ScenesPath();
    config.ResourcesPath();
    config.ShadersPath();
    config.ScriptsPath();
    return config;
}

std::filesystem::path ProjectConfig::FilePath() const { return root / FileName; }
std::filesystem::path ProjectConfig::ScenesPath() const { return NormalizeDirectory(root, scenes, "scene"); }
std::filesystem::path ProjectConfig::ResourcesPath() const { return NormalizeDirectory(root, resources, "resource"); }
std::filesystem::path ProjectConfig::ShadersPath() const { return NormalizeDirectory(root, shaders, "shader"); }
std::filesystem::path ProjectConfig::ScriptsPath() const { return NormalizeDirectory(root, scripts, "script"); }

void ProjectConfig::Save() const
{
    std::filesystem::create_directories(root);
    std::ofstream output(FilePath(), std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot write project file: " + FilePath().string());
    const nlohmann::json data = {
        {"version", 1}, {"name", name},
        {"scenes", scenes.generic_string()},
        {"resources", resources.generic_string()},
        {"shaders", shaders.generic_string()},
        {"scripts", scripts.generic_string()}
    };
    output << data.dump(2) << '\n';
    if (!output)
        throw std::runtime_error("Cannot finish writing project file: " + FilePath().string());
}

namespace AssetPaths
{
namespace
{
std::filesystem::path resourceRoot = "resource";
std::filesystem::path shaderRoot = "shaders";
std::filesystem::path scriptRoot = "scripts";
}

void Set(const std::filesystem::path& resources, const std::filesystem::path& shaders,
         const std::filesystem::path& scripts)
{
    resourceRoot = resources;
    shaderRoot = shaders;
    if (!scripts.empty())
        scriptRoot = scripts;
    else if (resources.has_parent_path())
        scriptRoot = resources.parent_path() / "scripts";
    else
        scriptRoot = "scripts";
}

std::filesystem::path Resource(const std::filesystem::path& relativePath)
{
    return resourceRoot / relativePath;
}

std::filesystem::path Shader(const std::filesystem::path& relativePath)
{
    return shaderRoot / relativePath;
}

std::filesystem::path Script(const std::filesystem::path& relativePath)
{
    return scriptRoot / relativePath;
}
}
