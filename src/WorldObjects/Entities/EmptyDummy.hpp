#pragma once
#include <string>
#include "WorldObjects/Entities/ThreeDObject.hpp"

class EmptyDummy : public ThreeDObject
{
public:
    EmptyDummy(int slotIndex = -1);

    std::string getName() const override;

    void initialize() override;
    void render(const glm::mat4&) override;

    void setIsOccupying(bool occ) { occupying = occ; }
    bool IsOccupying() const { return occupying; }

    void setSlot(int index);
    int getSlot() const;

    bool isDummy() const override { return true; }
    bool isInspectable() const override { return false; }

private:
    int slotIndex;
     bool occupying;
};
