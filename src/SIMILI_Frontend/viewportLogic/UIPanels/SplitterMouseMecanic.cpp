#include "SplitterMouseMecanic.hpp"
#include <cmath>

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

	dragBindedSplitters(splitters, dragged_index_, dx, dy);

	prev_mouse_x_ = mouseX;
	prev_mouse_y_ = mouseY;
}

void SplitterMouseMecanic::setAttachments(const std::vector<std::vector<int>>& attachments)
{
	attachments_ = attachments;
}

void SplitterMouseMecanic::dragBindedSplitters(std::vector<Splitter::SplitterData>& splitters, int draggedIndex, int dx, int dy)
{
	if (draggedIndex < 0 || draggedIndex >= static_cast<int>(attachments_.size()))
	{
		return;
	}

	const int ATTACH_TOLERANCE = 15;
	const auto& dragged = splitters[draggedIndex];
	const int originalY = dragged.y - dy;
	const int originalX = dragged.x - dx;

	for (int attachedIdx : attachments_[draggedIndex])
	{
		if (attachedIdx < 0 || attachedIdx >= static_cast<int>(splitters.size()))
		{
			continue;
		}

		auto& attached = splitters[attachedIdx];

		if (dragged.isHorizontal && attached.isVertical)
		{
			if (std::abs(attached.y - originalY) <= ATTACH_TOLERANCE)
			{
				attached.y += dy;
				attached.height -= dy;
			}
			else if (std::abs((attached.y + attached.height) - originalY) <= ATTACH_TOLERANCE)
			{
				attached.height += dy;
			}
		}
		else if (dragged.isVertical && attached.isHorizontal)
		{
			if (std::abs(attached.x - originalX) <= ATTACH_TOLERANCE)
			{
				attached.x += dx;
				attached.width -= dx;
			}
			else if (std::abs((attached.x + attached.width) - originalX) <= ATTACH_TOLERANCE)
			{
				attached.width += dx;
			}
		}
	}
}
