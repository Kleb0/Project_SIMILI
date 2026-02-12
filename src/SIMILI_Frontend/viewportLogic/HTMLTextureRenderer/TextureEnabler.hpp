#pragma once

#include "include/cef_task.h"
#include "include/cef_base.h"

class UIHandler;

class TextureEnablerTask : public CefTask
{
public:
	TextureEnablerTask(CefRefPtr<UIHandler> handler, bool enable);
	void Execute() override;

 private:
	CefRefPtr<UIHandler> handler_;
	bool enable_;

	IMPLEMENT_REFCOUNTING(TextureEnablerTask);
};