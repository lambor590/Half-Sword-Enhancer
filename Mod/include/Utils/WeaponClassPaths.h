#pragma once

#include <string>

struct WeaponClassPaths {
    std::string weaponClass;
    std::string headModule;
    std::string guardModule;
    std::string gripModule;
    std::string pommelModule;
    std::string subModule1;
    std::string subModule2;

    bool operator==(const WeaponClassPaths&) const = default;
};
