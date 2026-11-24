#pragma once

#include <optional>
#include <string>

class SoundManager;

class SoundManagerEditor
{
public:
    SoundManagerEditor();

    void Draw(bool& open);
    bool SaveForRun();

private:
    SoundManager* manager;
    std::optional<int> selected;
    std::optional<std::string> selectedAsset;
    std::string message;
    bool messageIsError = false;
};
