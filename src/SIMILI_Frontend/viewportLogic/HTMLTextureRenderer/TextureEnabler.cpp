#define GLM_ENABLE_EXPERIMENTAL

#include "TextureEnabler.hpp"
#include "SIMILI_Frontend/ui_handler.hpp"

TextureEnablerTask::TextureEnablerTask(CefRefPtr<UIHandler> handler, bool enable)
	: handler_(handler), enable_(enable)
{
}

void TextureEnablerTask::Execute()
{
	if (handler_)
	{
		handler_->enableSlotTextureRendering(enable_);
	}
}