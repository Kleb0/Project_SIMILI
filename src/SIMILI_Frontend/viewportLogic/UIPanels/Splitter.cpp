#include "Splitter.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

namespace
{
	constexpr int kDefaultSplitterThickness = 8;
	constexpr int kMinPanelWidth = 120;
	constexpr int kMinPanelHeight = 80;

	const char* splitterVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

void main()
{
	gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

	const char* splitterFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 splitterColor;

void main()
{
	FragColor = splitterColor;
}
)";
}

Splitter::Splitter()
	: window_(nullptr)
	, initialized_(false)
	, layout_ready_(false)
	, dragging_(false)
	, active_splitter_index_(-1)
	, hovered_splitter_index_(-1)
	, drag_anchor_(0)
	, last_window_width_(0)
	, last_window_height_(0)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
{
}

Splitter::~Splitter()
{
	shutdown();
}

bool Splitter::initialize(SDL_Window* window)
{
	if (initialized_)
	{
		window_ = window;
		return true;
	}

	if (!window)
	{
		return false;
	}

	window_ = window;

	if (!createGraphicsResources())
	{
		shutdown();
		return false;
	}

	SDL_GetWindowSize(window_, &last_window_width_, &last_window_height_);
	initialized_ = true;
	return true;
}

void Splitter::shutdown()
{
	destroyGraphicsResources();

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	panel_state_map_.clear();
	source_frame_data_map_.clear();
	splitters_.clear();
	window_ = nullptr;
	initialized_ = false;
	layout_ready_ = false;
	dragging_ = false;
	active_splitter_index_ = -1;
	hovered_splitter_index_ = -1;
	drag_anchor_ = 0;
	last_window_width_ = 0;
	last_window_height_ = 0;
}

bool Splitter::isReady() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	return initialized_ && layout_ready_;
}

void Splitter::syncFrameDatas(const SIMILI::Frontend::FrameDatas* frameDatas)
{
	if (!initialized_ || !frameDatas)
	{
		return;
	}

	const auto& frameDataMap = frameDatas->getFrameData();
	if (frameDataMap.empty())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	int windowWidth = 0;
	int windowHeight = 0;
	if (window_)
	{
		SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
	}

	if (hasSourceGeometryChanged(frameDataMap) || shouldRefreshFromSource(frameDataMap))
	{
		source_frame_data_map_ = frameDataMap;
		rebuildFromSource(frameDataMap);
	}
	else if (windowWidth > 0 && windowHeight > 0 && (windowWidth != last_window_width_ || windowHeight != last_window_height_))
	{
		scaleLayoutToWindow(windowWidth, windowHeight);
		refreshDerivedData();
	}
	else
	{
		refreshDerivedData();
	}
}

bool Splitter::handleEvent(const SDL_Event& event)
{
	if (!initialized_)
	{
		return false;
	}

	int mouseX = 0;
	int mouseY = 0;
	resolveMousePosition(event, mouseX, mouseY);

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	if (!layout_ready_)
	{
		return false;
	}

	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
		{
			if (window_)
			{
				int windowWidth = 0;
				int windowHeight = 0;
				SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
				if (windowWidth > 0 && windowHeight > 0 && (windowWidth != last_window_width_ || windowHeight != last_window_height_))
				{
					scaleLayoutToWindow(windowWidth, windowHeight);
					refreshDerivedData();
				}
			}

			return false;
		}

		case SDL_EVENT_MOUSE_MOTION:
		{
			if (dragging_ && active_splitter_index_ >= 0 && active_splitter_index_ < static_cast<int>(splitters_.size()))
			{
				const SplitterGeometry activeSplitter = splitters_[active_splitter_index_];
				int currentAnchor = activeSplitter.axis == Axis::Vertical ? mouseX : mouseY;
				int delta = currentAnchor - drag_anchor_;
				if (delta != 0)
				{
					int appliedDelta = applyDelta(activeSplitter, delta);
					if (appliedDelta != 0)
					{
						drag_anchor_ += appliedDelta;
						refreshDerivedData();
					}
				}
				hovered_splitter_index_ = active_splitter_index_;
				return true;
			}

			hovered_splitter_index_ = pickSplitterIndex(mouseX, mouseY);
			return hovered_splitter_index_ != -1;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			if (event.button.button != SDL_BUTTON_LEFT)
			{
				return false;
			}

			const int splitterIndex = pickSplitterIndex(mouseX, mouseY);
			if (splitterIndex == -1)
			{
				return false;
			}

			beginDrag(splitterIndex, mouseX, mouseY);
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			if (event.button.button != SDL_BUTTON_LEFT)
			{
				return false;
			}

			if (!dragging_)
			{
				return false;
			}

			endDrag();
			hovered_splitter_index_ = pickSplitterIndex(mouseX, mouseY);
			return true;
		}

		default:
		{
			return false;
		}
	}
}

bool Splitter::hasSourceGeometryChanged(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const
{
	if (source_frame_data_map_.size() != frameDataMap.size())
	{
		return true;
	}

	for (const auto& pair : frameDataMap)
	{
		auto it = source_frame_data_map_.find(pair.first);
		if (it == source_frame_data_map_.end())
		{
			return true;
		}

		const SIMILI::Frontend::IFrameScreenData& current = pair.second;
		const SIMILI::Frontend::IFrameScreenData& previous = it->second;
		if (current.relativeX != previous.relativeX || current.relativeY != previous.relativeY || current.width != previous.width || current.height != previous.height)
		{
			return true;
		}
	}

	return false;
}

void Splitter::draw()
{
	if (!initialized_)
	{
		return;
	}

	int drawableWidth = 0;
	int drawableHeight = 0;
	std::vector<SplitterGeometry> splitters;
	int hoveredSplitterIndex = -1;
	int activeSplitterIndex = -1;
	bool dragging = false;

	{
		std::lock_guard<std::mutex> lock(splitter_mutex_);
		if (!layout_ready_ || splitters_.empty())
		{
			return;
		}

		splitters = splitters_;
		hoveredSplitterIndex = hovered_splitter_index_;
		activeSplitterIndex = active_splitter_index_;
		dragging = dragging_;
	}

	SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight);
	if (drawableWidth <= 0 || drawableHeight <= 0)
	{
		return;
	}

	for (std::size_t index = 0; index < splitters.size(); ++index)
	{
		const bool highlighted = static_cast<int>(index) == hoveredSplitterIndex || (dragging && static_cast<int>(index) == activeSplitterIndex);
		const glm::vec4 color = highlighted ? glm::vec4(0.12f, 0.78f, 0.24f, 1.0f) : glm::vec4(0.22f, 0.24f, 0.26f, 1.0f);
		drawGeometry(splitters[index], drawableWidth, drawableHeight, color.r, color.g, color.b, color.a);
	}
}

bool Splitter::getViewportFrameData(SIMILI::Frontend::IFrameScreenData& outData) const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	auto it = panel_state_map_.find("viewport_panel");
	if (it == panel_state_map_.end())
	{
		return false;
	}

	outData = it->second.frame;
	return true;
}

std::map<std::string, SIMILI::Frontend::IFrameScreenData> Splitter::getUIPanelFrameDatas() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> frameDataMap;

	for (const auto& pair : panel_state_map_)
	{
		if (pair.first == "viewport_panel")
		{
			continue;
		}

		frameDataMap[pair.first] = pair.second.frame;
	}

	return frameDataMap;
}

std::map<std::string, SIMILI::Frontend::IFrameScreenData> Splitter::getAllFrameDatas() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> frameDataMap;

	for (const auto& pair : panel_state_map_)
	{
		frameDataMap[pair.first] = pair.second.frame;
	}

	return frameDataMap;
}

bool Splitter::createGraphicsResources()
{
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &splitterVertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	GLint success = 0;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glDeleteShader(vertexShader);
		return false;
	}

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &splitterFragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return false;
	}

	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (!success)
	{
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
		return false;
	}

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);

	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return vao_ != 0 && vbo_ != 0;
}

void Splitter::destroyGraphicsResources()
{
	if (vbo_)
	{
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}

	if (vao_)
	{
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}

	if (shader_program_)
	{
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}
}

bool Splitter::shouldRefreshFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const
{
	if (!layout_ready_)
	{
		return true;
	}

	if (panel_state_map_.size() != frameDataMap.size())
	{
		return true;
	}

	for (const auto& pair : frameDataMap)
	{
		if (panel_state_map_.find(pair.first) == panel_state_map_.end())
		{
			return true;
		}
	}

	return false;
}

void Splitter::rebuildFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap)
{
	panel_state_map_.clear();

	for (const auto& pair : frameDataMap)
	{
		PanelState state;
		state.frame = pair.second;
		state.client_offset_x = pair.second.clientX - pair.second.relativeX;
		state.client_offset_y = pair.second.clientY - pair.second.relativeY;
		panel_state_map_[pair.first] = state;
	}

	if (window_)
	{
		SDL_GetWindowSize(window_, &last_window_width_, &last_window_height_);
	}

	layout_ready_ = !panel_state_map_.empty();
	refreshDerivedData();
}

void Splitter::scaleLayoutToWindow(int newWindowWidth, int newWindowHeight)
{
	if (newWindowWidth <= 0 || newWindowHeight <= 0)
	{
		return;
	}

	if (last_window_width_ <= 0 || last_window_height_ <= 0)
	{
		last_window_width_ = newWindowWidth;
		last_window_height_ = newWindowHeight;
		return;
	}

	const float scaleX = static_cast<float>(newWindowWidth) / static_cast<float>(last_window_width_);
	const float scaleY = static_cast<float>(newWindowHeight) / static_cast<float>(last_window_height_);
	if (scaleX <= 0.0f || scaleY <= 0.0f)
	{
		return;
	}

	float dpiScale = 1.0f;
	if (window_)
	{
		int drawableWidth = 0;
		int drawableHeight = 0;
		SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight);
		if (newWindowWidth > 0 && drawableWidth > 0)
		{
			dpiScale = static_cast<float>(drawableWidth) / static_cast<float>(newWindowWidth);
		}
	}

	for (auto& pair : panel_state_map_)
	{
		SIMILI::Frontend::IFrameScreenData& frame = pair.second.frame;
		frame.relativeX = static_cast<int>(std::lround(static_cast<float>(frame.relativeX) * scaleX));
		frame.relativeY = static_cast<int>(std::lround(static_cast<float>(frame.relativeY) * scaleY));
		frame.width = std::max(1, static_cast<int>(std::lround(static_cast<float>(frame.width) * scaleX)));
		frame.height = std::max(1, static_cast<int>(std::lround(static_cast<float>(frame.height) * scaleY)));
		frame.windowWidth = newWindowWidth;
		frame.windowHeight = newWindowHeight;
		frame.dpiScale = dpiScale;
	}

	last_window_width_ = newWindowWidth;
	last_window_height_ = newWindowHeight;
}

void Splitter::refreshDerivedData()
{
	updateClientCoordinates();
	rebuildSplitters();
	layout_ready_ = !panel_state_map_.empty();
	if (splitters_.empty())
	{
		hovered_splitter_index_ = -1;
		active_splitter_index_ = dragging_ ? active_splitter_index_ : -1;
	}
}

void Splitter::rebuildSplitters()
{
	splitters_.clear();

	auto getPanel = [this](const std::string& panelName) -> const PanelState*
	{
		auto it = panel_state_map_.find(panelName);
		if (it == panel_state_map_.end())
		{
			return nullptr;
		}

		return &it->second;
	};

	const PanelState* hierarchyPanel = getPanel("hierarchy_panel");
	const PanelState* viewportPanel = getPanel("viewport_panel");
	const PanelState* objectInspectorPanel = getPanel("object_inspector_panel");
	const PanelState* historyPanel = getPanel("history_panel");
	const PanelState* projectViewerPanel = getPanel("project_viewer_panel");

	int topRowTop = 0;
	int topRowBottom = 0;
	int topRowLeft = 0;
	int topRowRight = 0;
	bool hasTopRowBounds = false;

	for (const auto* panel : {hierarchyPanel, viewportPanel, objectInspectorPanel, historyPanel})
	{
		if (!panel)
		{
			continue;
		}

		const int left = panel->frame.relativeX;
		const int top = panel->frame.relativeY;
		const int right = panel->frame.relativeX + panel->frame.width;
		const int bottom = panel->frame.relativeY + panel->frame.height;

		if (!hasTopRowBounds)
		{
			topRowLeft = left;
			topRowTop = top;
			topRowRight = right;
			topRowBottom = bottom;
			hasTopRowBounds = true;
		}
		else
		{
			topRowLeft = std::min(topRowLeft, left);
			topRowTop = std::min(topRowTop, top);
			topRowRight = std::max(topRowRight, right);
			topRowBottom = std::max(topRowBottom, bottom);
		}
	}

	auto addVerticalSplitter = [this, topRowTop, topRowBottom](Type type, const PanelState* leftPanel, const PanelState* rightPanel)
	{
		if (!leftPanel || !rightPanel)
		{
			return;
		}

		const int boundary = leftPanel->frame.relativeX + leftPanel->frame.width;
		const int gap = rightPanel->frame.relativeX - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int x = gap > 0 ? boundary : boundary - thickness / 2;
		const int height = std::max(0, topRowBottom - topRowTop);
		if (height <= 0)
		{
			return;
		}

		splitters_.push_back({type, Axis::Vertical, x, topRowTop, thickness, height});
	};

	auto addHorizontalSplitter = [this](Type type, const PanelState* topPanel, const PanelState* bottomPanel)
	{
		if (!topPanel || !bottomPanel)
		{
			return;
		}

		const int boundary = topPanel->frame.relativeY + topPanel->frame.height;
		const int gap = bottomPanel->frame.relativeY - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int y = gap > 0 ? boundary : boundary - thickness / 2;
		const int left = std::min(topPanel->frame.relativeX, bottomPanel->frame.relativeX);
		const int right = std::max(topPanel->frame.relativeX + topPanel->frame.width, bottomPanel->frame.relativeX + bottomPanel->frame.width);
		const int width = std::max(0, right - left);
		if (width <= 0)
		{
			return;
		}

		splitters_.push_back({type, Axis::Horizontal, left, y, width, thickness});
	};

	if (hasTopRowBounds)
	{
		addVerticalSplitter(Type::HierarchyViewport, hierarchyPanel, viewportPanel);
		addVerticalSplitter(Type::ViewportInspector, viewportPanel, objectInspectorPanel);
	}

	addHorizontalSplitter(Type::InspectorHistory, objectInspectorPanel, historyPanel);

	if (hasTopRowBounds && projectViewerPanel)
	{
		const int boundary = topRowBottom;
		const int gap = projectViewerPanel->frame.relativeY - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int y = gap > 0 ? boundary : boundary - thickness / 2;
		const int left = std::min(topRowLeft, projectViewerPanel->frame.relativeX);
		const int right = std::max(topRowRight, projectViewerPanel->frame.relativeX + projectViewerPanel->frame.width);
		const int width = std::max(0, right - left);
		if (width > 0)
		{
			splitters_.push_back({Type::TopProjectViewer, Axis::Horizontal, left, y, width, thickness});
		}
	}
}

bool Splitter::resolveMousePosition(const SDL_Event& event, int& x, int& y) const
{
	switch (event.type)
	{
		case SDL_EVENT_MOUSE_MOTION:
		{
			x = static_cast<int>(std::lround(event.motion.x));
			y = static_cast<int>(std::lround(event.motion.y));
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			x = static_cast<int>(std::lround(event.button.x));
			y = static_cast<int>(std::lround(event.button.y));
			return true;
		}

		default:
		{
			float mouseX = 0.0f;
			float mouseY = 0.0f;
			SDL_GetMouseState(&mouseX, &mouseY);
			x = static_cast<int>(std::lround(mouseX));
			y = static_cast<int>(std::lround(mouseY));
			return false;
		}
	}
}

int Splitter::pickSplitterIndex(int x, int y) const
{
	for (std::size_t index = 0; index < splitters_.size(); ++index)
	{
		const SplitterGeometry& splitter = splitters_[index];
		const bool insideX = x >= splitter.x && x <= splitter.x + splitter.width;
		const bool insideY = y >= splitter.y && y <= splitter.y + splitter.height;
		if (insideX && insideY)
		{
			return static_cast<int>(index);
		}
	}

	return -1;
}

void Splitter::beginDrag(int splitterIndex, int x, int y)
{
	if (splitterIndex < 0 || splitterIndex >= static_cast<int>(splitters_.size()))
	{
		return;
	}

	dragging_ = true;
	active_splitter_index_ = splitterIndex;
	hovered_splitter_index_ = splitterIndex;
	drag_anchor_ = splitters_[splitterIndex].axis == Axis::Vertical ? x : y;
}

void Splitter::endDrag()
{
	dragging_ = false;
	active_splitter_index_ = -1;
	drag_anchor_ = 0;
}

void Splitter::updateClientCoordinates()
{
	for (auto& pair : panel_state_map_)
	{
		pair.second.frame.clientX = pair.second.frame.relativeX + pair.second.client_offset_x;
		pair.second.frame.clientY = pair.second.frame.relativeY + pair.second.client_offset_y;
	}
}

int Splitter::applyDelta(const SplitterGeometry& splitter, int delta)
{
	switch (splitter.type)
	{
		case Type::HierarchyViewport:
		{
			return applyVerticalDelta("hierarchy_panel", "viewport_panel", delta, {});
		}

		case Type::ViewportInspector:
		{
			return applyVerticalDelta("viewport_panel", "object_inspector_panel", delta, {"history_panel"});
		}

		case Type::InspectorHistory:
		{
			return applyHorizontalDelta("object_inspector_panel", "history_panel", delta);
		}

		case Type::TopProjectViewer:
		{
			return applyTopRowDelta(delta);
		}
	}

	return 0;
}

int Splitter::applyVerticalDelta(const std::string& leftPanelName, const std::string& rightPanelName, int delta, const std::vector<std::string>& linkedRightPanels)
{
	auto leftIt = panel_state_map_.find(leftPanelName);
	auto rightIt = panel_state_map_.find(rightPanelName);
	if (leftIt == panel_state_map_.end() || rightIt == panel_state_map_.end())
	{
		return 0;
	}

	int maxPositiveDelta = rightIt->second.frame.width - kMinPanelWidth;
	for (const auto& panelName : linkedRightPanels)
	{
		auto linkedIt = panel_state_map_.find(panelName);
		if (linkedIt != panel_state_map_.end())
		{
			maxPositiveDelta = std::min(maxPositiveDelta, linkedIt->second.frame.width - kMinPanelWidth);
		}
	}

	const int maxNegativeDelta = leftIt->second.frame.width - kMinPanelWidth;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	leftIt->second.frame.width += clampedDelta;
	rightIt->second.frame.relativeX += clampedDelta;
	rightIt->second.frame.width -= clampedDelta;

	for (const auto& panelName : linkedRightPanels)
	{
		auto linkedIt = panel_state_map_.find(panelName);
		if (linkedIt != panel_state_map_.end())
		{
			linkedIt->second.frame.relativeX += clampedDelta;
			linkedIt->second.frame.width -= clampedDelta;
		}
	}

	return clampedDelta;
}

int Splitter::applyHorizontalDelta(const std::string& topPanelName, const std::string& bottomPanelName, int delta)
{
	auto topIt = panel_state_map_.find(topPanelName);
	auto bottomIt = panel_state_map_.find(bottomPanelName);
	if (topIt == panel_state_map_.end() || bottomIt == panel_state_map_.end())
	{
		return 0;
	}

	const int maxNegativeDelta = topIt->second.frame.height - kMinPanelHeight;
	const int maxPositiveDelta = bottomIt->second.frame.height - kMinPanelHeight;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	topIt->second.frame.height += clampedDelta;
	bottomIt->second.frame.relativeY += clampedDelta;
	bottomIt->second.frame.height -= clampedDelta;
	return clampedDelta;
}

int Splitter::applyTopRowDelta(int delta)
{
	auto hierarchyIt = panel_state_map_.find("hierarchy_panel");
	auto viewportIt = panel_state_map_.find("viewport_panel");
	auto objectInspectorIt = panel_state_map_.find("object_inspector_panel");
	auto historyIt = panel_state_map_.find("history_panel");
	auto projectViewerIt = panel_state_map_.find("project_viewer_panel");

	if (hierarchyIt == panel_state_map_.end() || viewportIt == panel_state_map_.end() || objectInspectorIt == panel_state_map_.end() || historyIt == panel_state_map_.end() || projectViewerIt == panel_state_map_.end())
	{
		return 0;
	}

	const int rightGap = historyIt->second.frame.relativeY - (objectInspectorIt->second.frame.relativeY + objectInspectorIt->second.frame.height);
	const int rightTotalHeight = objectInspectorIt->second.frame.height + rightGap + historyIt->second.frame.height;
	const int minRightTotalHeight = kMinPanelHeight + rightGap + kMinPanelHeight;

	const int maxNegativeDelta = std::min({
		hierarchyIt->second.frame.height - kMinPanelHeight,
		viewportIt->second.frame.height - kMinPanelHeight,
		rightTotalHeight - minRightTotalHeight
	});
	const int maxPositiveDelta = projectViewerIt->second.frame.height - kMinPanelHeight;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	hierarchyIt->second.frame.height += clampedDelta;
	viewportIt->second.frame.height += clampedDelta;

	const int currentRightContentHeight = objectInspectorIt->second.frame.height + historyIt->second.frame.height;
	const int newRightTotalHeight = rightTotalHeight + clampedDelta;
	const int newRightContentHeight = newRightTotalHeight - rightGap;
	float ratio = 0.5f;
	if (currentRightContentHeight > 0)
	{
		ratio = static_cast<float>(objectInspectorIt->second.frame.height) / static_cast<float>(currentRightContentHeight);
	}

	int newObjectInspectorHeight = static_cast<int>(std::lround(static_cast<float>(newRightContentHeight) * ratio));
	newObjectInspectorHeight = std::clamp(newObjectInspectorHeight, kMinPanelHeight, newRightContentHeight - kMinPanelHeight);
	const int newHistoryHeight = newRightContentHeight - newObjectInspectorHeight;

	objectInspectorIt->second.frame.height = newObjectInspectorHeight;
	historyIt->second.frame.relativeY = objectInspectorIt->second.frame.relativeY + objectInspectorIt->second.frame.height + rightGap;
	historyIt->second.frame.height = newHistoryHeight;

	projectViewerIt->second.frame.relativeY += clampedDelta;
	projectViewerIt->second.frame.height -= clampedDelta;

	return clampedDelta;
}

void Splitter::drawGeometry(const SplitterGeometry& splitter, int drawableWidth, int drawableHeight, float red, float green, float blue, float alpha)
{
	if (!window_ || !shader_program_ || !vao_ || !vbo_)
	{
		return;
	}

	int logicalWidth = 0;
	int logicalHeight = 0;
	SDL_GetWindowSize(window_, &logicalWidth, &logicalHeight);
	if (logicalWidth <= 0 || logicalHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		return;
	}

	const float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWidth);
	const float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalHeight);
	const int x = static_cast<int>(std::lround(static_cast<float>(splitter.x) * scaleX));
	const int y = static_cast<int>(std::lround(static_cast<float>(splitter.y) * scaleY));
	const int width = std::max(1, static_cast<int>(std::lround(static_cast<float>(splitter.width) * scaleX)));
	const int height = std::max(1, static_cast<int>(std::lround(static_cast<float>(splitter.height) * scaleY)));

	const float left = (static_cast<float>(x) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	const float right = (static_cast<float>(x + width) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	const float top = 1.0f - (static_cast<float>(y) / static_cast<float>(drawableHeight)) * 2.0f;
	const float bottom = 1.0f - (static_cast<float>(y + height) / static_cast<float>(drawableHeight)) * 2.0f;

	const float vertices[] = {
		left, top,
		left, bottom,
		right, bottom,
		left, top,
		right, bottom,
		right, top
	};

	glUseProgram(shader_program_);
	glUniform4f(glGetUniformLocation(shader_program_, "splitterColor"), red, green, blue, alpha);
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}
