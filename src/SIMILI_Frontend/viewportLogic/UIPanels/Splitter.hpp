#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class Splitter
{
public:
	Splitter();
	~Splitter();

	bool initialize(SDL_Window* window);
	void shutdown();
	void syncFrameDatas(const SIMILI::Frontend::FrameDatas* frameDatas);
	bool handleEvent(const SDL_Event& event);
	void draw();

	bool getViewportFrameData(SIMILI::Frontend::IFrameScreenData& outData) const;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> getUIPanelFrameDatas() const;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> getAllFrameDatas() const;
	bool isReady() const;
	bool isDragging() const { return dragging_; }

private:
	enum class Axis
	{
		Vertical,
		Horizontal
	};

	enum class Type
	{
		HierarchyViewport,
		ViewportInspector,
		InspectorHistory,
		TopProjectViewer
	};

	struct SplitterGeometry
	{
		Type type;
		Axis axis;
		int x;
		int y;
		int width;
		int height;
	};

	struct PanelState
	{
		SIMILI::Frontend::IFrameScreenData frame;
		int client_offset_x;
		int client_offset_y;
	};

	SDL_Window* window_;
	bool initialized_;
	bool layout_ready_;
	bool dragging_;
	int active_splitter_index_;
	int hovered_splitter_index_;
	int drag_anchor_;
	int last_window_width_;
	int last_window_height_;
	GLuint vao_;
	GLuint vbo_;
	GLuint shader_program_;
	std::map<std::string, PanelState> panel_state_map_;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> source_frame_data_map_;
	std::vector<SplitterGeometry> splitters_;
	mutable std::mutex splitter_mutex_;

	bool createGraphicsResources();
	void destroyGraphicsResources();
	bool hasSourceGeometryChanged(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const;
	bool shouldRefreshFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const;
	void rebuildFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap);
	void scaleLayoutToWindow(int newWindowWidth, int newWindowHeight);
	void refreshDerivedData();
	void rebuildSplitters();
	bool resolveMousePosition(const SDL_Event& event, int& x, int& y) const;
	int pickSplitterIndex(int x, int y) const;
	void beginDrag(int splitterIndex, int x, int y);
	void endDrag();
	void updateClientCoordinates();
	int applyDelta(const SplitterGeometry& splitter, int delta);
	int applyVerticalDelta(const std::string& leftPanelName, const std::string& rightPanelName, int delta, const std::vector<std::string>& linkedRightPanels);
	int applyHorizontalDelta(const std::string& topPanelName, const std::string& bottomPanelName, int delta);
	int applyTopRowDelta(int delta);
	void drawGeometry(const SplitterGeometry& splitter, int drawableWidth, int drawableHeight, float red, float green, float blue, float alpha);
};
