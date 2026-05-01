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
	void dragSplitter(std::vector<Splitter::SplitterData>& splitters, int mouseX, int mouseY, bool isLeftButtonDown);
	int getHoveredIndex() const;
	int getDraggedIndex() const;

	Color getHoverColor() const;
	Color getDefaultColor() const;

private:
	int hovered_index_;
	int dragged_index_;
	int prev_mouse_x_;
	int prev_mouse_y_;
	bool is_dragging_;
};
