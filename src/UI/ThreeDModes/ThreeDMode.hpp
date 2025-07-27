#pragma once
#include <vector>
#include <memory>


class ThreeDMode {
public:
    virtual ~ThreeDMode() = default;

    // Obligatoire pour chaque mode concret
    virtual const char* getName() const = 0;

    // Définit le mode courant
    static void setMode(std::unique_ptr<ThreeDMode> newMode);

    // Récupère le mode courant
    static ThreeDMode* getCurrentMode();

private:
    inline static std::unique_ptr<ThreeDMode> currentMode = nullptr;
};