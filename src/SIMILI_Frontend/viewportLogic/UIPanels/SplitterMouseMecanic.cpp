#include "SplitterMouseMecanic.hpp"

SplitterMouseMecanic::SplitterMouseMecanic()
	: hovered_index_(-1)
	, dragged_index_(-1)
	, prev_mouse_x_(0)
	, prev_mouse_y_(0)
	, is_dragging_(false)
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

int SplitterMouseMecanic::getDraggedIndex() const
{
	return dragged_index_;
}

SplitterMouseMecanic::Color SplitterMouseMecanic::getHoverColor() const
{
	return { 0.0f, 0.4f, 1.0f, 1.0f };
}

SplitterMouseMecanic::Color SplitterMouseMecanic::getDefaultColor() const
{
	return { 0.0f, 1.0f, 0.0f, 1.0f };
}

void SplitterMouseMecanic::dragSplitter(std::vector<Splitter::SplitterData>& splitters, int mouseX, int mouseY, bool isLeftButtonDown)
{
	if (!isLeftButtonDown)
	{
		is_dragging_ = false;
		dragged_index_ = -1;
		prev_mouse_x_ = mouseX;
		prev_mouse_y_ = mouseY;
		return;
	}

	if (!is_dragging_)
	{
		if (hovered_index_ >= 0)
		{
			is_dragging_ = true;
			dragged_index_ = hovered_index_;
		}
		prev_mouse_x_ = mouseX;
		prev_mouse_y_ = mouseY;
		return;
	}

	if (dragged_index_ < 0 || dragged_index_ >= static_cast<int>(splitters.size()))
	{
		return;
	}

	const int dx = mouseX - prev_mouse_x_;
	const int dy = mouseY - prev_mouse_y_;

	auto& s = splitters[dragged_index_];

	if (s.isVertical)
	{
		s.x += dx;
	}
	else if (s.isHorizontal)
	{
		s.y += dy;
	}

	prev_mouse_x_ = mouseX;
	prev_mouse_y_ = mouseY;
}
