#include "SplitterMouseMecanic.hpp"

SplitterMouseMecanic::SplitterMouseMecanic()
	: hovered_index_(-1)
{
}

void SplitterMouseMecanic::update(const std::vector<Splitter::SplitterData>& splitters, int mouseX, int mouseY)
{
	hovered_index_ = -1;

	for (int i = 0; i < static_cast<int>(splitters.size()); ++i)
	{
		const auto& s = splitters[i];
		if (mouseX >= s.x && mouseX < s.x + s.width &&
			mouseY >= s.y && mouseY < s.y + s.height)
		{
			hovered_index_ = i;
			return;
		}
	}
}

int SplitterMouseMecanic::getHoveredIndex() const
{
	return hovered_index_;
}

SplitterMouseMecanic::Color SplitterMouseMecanic::getHoverColor() const
{
	return { 0.0f, 0.4f, 1.0f, 1.0f };
}

SplitterMouseMecanic::Color SplitterMouseMecanic::getDefaultColor() const
{
	return { 0.0f, 1.0f, 0.0f, 1.0f };
}
