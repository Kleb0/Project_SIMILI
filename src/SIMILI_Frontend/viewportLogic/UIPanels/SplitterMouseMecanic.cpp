#include "SplitterMouseMecanic.hpp"
#include <cmath>

SplitterMouseMecanic::SplitterMouseMecanic()
	: hovered_index_(-1)
	, dragged_index_(-1)
	, prev_mouse_x_(0)
	, prev_mouse_y_(0)
	, is_dragging_(false)
	, is_operating_(false)
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

bool SplitterMouseMecanic::isOperating() const
{
	return is_operating_;
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
		is_operating_ = false;
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
			is_operating_ = true;
			dragged_index_ = hovered_index_;
		}
		else
		{
			is_operating_ = false;
		}
		prev_mouse_x_ = mouseX;
		prev_mouse_y_ = mouseY;
		return;
	}

	if (dragged_index_ < 0 || dragged_index_ >= static_cast<int>(splitters.size()))
	{
		is_operating_ = false;
		return;
	}

	const int dx = mouseX - prev_mouse_x_;
	const int dy = mouseY - prev_mouse_y_;

	const int STOP_MARGIN = 5;
	int clampedDx = dx;
	int clampedDy = dy;

	const auto& s_ref = splitters[dragged_index_];
	for (int i = 0; i < static_cast<int>(splitters.size()); ++i)
	{
		if (i == dragged_index_) continue;
		const auto& other = splitters[i];
		if (s_ref.isVertical != other.isVertical) continue;

		if (s_ref.isVertical)
		{
			const bool yOverlap = (s_ref.y < other.y + other.height) && (s_ref.y + s_ref.height > other.y);
			if (!yOverlap) continue;

			if (clampedDx > 0)
			{
				const int gap = other.x - (s_ref.x + s_ref.width);
				if (gap >= 0 && gap - clampedDx < STOP_MARGIN)
					clampedDx = std::max(0, gap - STOP_MARGIN);
			}
			else if (clampedDx < 0)
			{
				const int gap = s_ref.x - (other.x + other.width);
				if (gap >= 0 && gap + clampedDx < STOP_MARGIN)
					clampedDx = std::min(0, -(gap - STOP_MARGIN));
			}
		}
		else
		{
			const bool xOverlap = (s_ref.x < other.x + other.width) && (s_ref.x + s_ref.width > other.x);
			if (!xOverlap) continue;

			if (clampedDy > 0)
			{
				const int gap = other.y - (s_ref.y + s_ref.height);
				if (gap >= 0 && gap - clampedDy < STOP_MARGIN)
					clampedDy = std::max(0, gap - STOP_MARGIN);
			}
			else if (clampedDy < 0)
			{
				const int gap = s_ref.y - (other.y + other.height);
				if (gap >= 0 && gap + clampedDy < STOP_MARGIN)
					clampedDy = std::min(0, -(gap - STOP_MARGIN));
			}
		}
	}

	auto& s = splitters[dragged_index_];

	if (s.isVertical)
	{
		s.x += clampedDx;
	}
	else if (s.isHorizontal)
	{
		s.y += clampedDy;
	}

	dragBindedSplitters(splitters, dragged_index_, clampedDx, clampedDy);
	is_operating_ = true;

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
			const int distTop    = std::abs(attached.y - originalY);
			const int distBottom = std::abs((attached.y + attached.height) - originalY);
			if (distTop <= distBottom)
			{
				attached.y += dy;
				attached.height -= dy;
			}
			else
			{
				attached.height += dy;
			}
		}
		else if (dragged.isVertical && attached.isHorizontal)
		{
			const int distLeft  = std::abs(attached.x - originalX);
			const int distRight = std::abs((attached.x + attached.width) - originalX);
			if (distLeft <= distRight)
			{
				attached.x += dx;
				attached.width -= dx;
			}
			else
			{
				attached.width += dx;
			}
		}
	}
}
