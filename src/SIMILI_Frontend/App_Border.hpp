#pragma once

enum class BorderState
{
	Init,
	Maximized,
	Reduced,
	Updating
};

class App_Border
{
public:
	App_Border();
	~App_Border();

	void updateDimensions(int windowWidth, int windowHeight);

	int getLeft() const;
	int getRight() const;
	int getTop() const;
	int getBottom() const;

	int getWidth() const;
	int getHeight() const;

	int getReferenceWindowWidth() const;
	int getReferenceWindowHeight() const;

	void enableDebugLine(bool enabled);
	bool isDebugLineEnabled() const;

	BorderState getCurrentState() const;
	void setState(BorderState newState);

private:
	static constexpr int BORDER_OFFSET = 3;

	int left_;
	int right_;
	int top_;
	int bottom_;
	int width_;
	int height_;
	int reference_window_width_;
	int reference_window_height_;
	bool first_update_;
	bool debug_line_enabled_;
	BorderState current_state_;
};
