#pragma once

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

	void enableDebugLine(bool enabled);
	bool isDebugLineEnabled() const;

private:
	static constexpr int BORDER_OFFSET = 3;

	int left_;
	int right_;
	int top_;
	int bottom_;
	int width_;
	int height_;
	bool first_update_;
	bool debug_line_enabled_;
};
