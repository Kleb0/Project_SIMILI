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
	void dragBindedSplitters(std::vector<Splitter::SplitterData>& splitters, int draggedIndex, int dx, int dy);
	void setAttachments(const std::vector<std::vector<int>>& attachments);
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
	std::vector<std::vector<int>> attachments_;
};
