#pragma once

#include "Splitter.hpp"
#include <vector>

class SplitterMouseMecanic
{
public:
	struct Color
	{
		float r, g, b, a;
	};

	SplitterMouseMecanic();
	~SplitterMouseMecanic() = default;

	void update(const std::vector<Splitter::SplitterData>& splitters, int mouseX, int mouseY);
	int getHoveredIndex() const;

	Color getHoverColor() const;
	Color getDefaultColor() const;

private:
	int hovered_index_;
};
