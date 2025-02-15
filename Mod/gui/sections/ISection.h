#pragma once
#include "MenuSection.h"

class ISection {
public:
    virtual ~ISection() = default;
    virtual void Setup(MenuSection& section) = 0;
};