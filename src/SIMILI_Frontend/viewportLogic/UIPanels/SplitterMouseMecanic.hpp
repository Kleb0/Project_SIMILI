#pragma once

#include "Splitter.hpp"
#include <vector>

class SplitterMouseMecanic
{
public:
	SplitterMouseMecanic();
	~SplitterMouseMecanic() = default;

	void update(const std::vector<Splitter::SplitterData>& splitters, int mouseX, int mouseY);
	int getHoveredIndex() const;

private:
	int hovered_index_;
};
