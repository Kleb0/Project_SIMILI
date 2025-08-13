#include "WorldObjects/Entities/EmptyDummy.hpp"

EmptyDummy::EmptyDummy(int slotIndex)
    : slotIndex(slotIndex) {}

std::string EmptyDummy::getName() const
{
    return "Empty Slot";
}

void EmptyDummy::initialize()
{
    // It's a placeholder, no initialization needed
}

void EmptyDummy::render(const glm::mat4&)
{
   
}

void EmptyDummy::setSlot(int index)
{
    slotIndex = index;
}

int EmptyDummy::getSlot() const
{
    return slotIndex;
}
