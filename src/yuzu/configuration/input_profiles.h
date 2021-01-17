// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "yuzu/configuration/config.h"

class InputProfiles {

public:
    explicit InputProfiles();
    ~InputProfiles() = default;

    std::vector<std::string> GetInputProfileNames();

    bool IsProfileNameValid(std::string profile_name);

    bool CreateProfile(std::string profile_name, std::size_t player_index);
    bool DeleteProfile(std::string profile_name);
    bool LoadProfile(std::string profile_name, std::size_t player_index);
    bool SaveProfile(std::string profile_name, std::size_t player_index);

private:
    bool ProfileExistsInMap(std::string profile_name);
    bool ProfileExistsInFilesystem(std::string profile_name);
    bool IsINI(std::string filename);
    std::string GetNameWithoutExtension(std::string filename);

    std::unordered_map<std::string, std::unique_ptr<Config>> map_profiles;
};
