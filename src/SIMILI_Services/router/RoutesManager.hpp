#pragma once

#include "RouterSim.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../SIMILI_Frontend/UI_Engine/ui_handler.hpp"

struct GLFWwindow;

namespace SIMILI 
{
	namespace Router 
	{

		class RoutesManager 
		{
			public:
				RoutesManager() = default;
				~RoutesManager() = default;

				void initializeRoutes(
					RouterSim& router,
					OpenGLContext& renderer,
					ThreeDScene& scene,
					CefRefPtr<UIHandler>& handler,
					GLFWwindow* glfwWindow
				);

			private:
				void registerContextRoutes(RouterSim& router, OpenGLContext& renderer);
				void registerSceneRoutes(RouterSim& router, ThreeDScene& scene, OpenGLContext& renderer);
				void registerObjectRoutes(RouterSim& router, ThreeDScene& scene, CefRefPtr<UIHandler>& handler, GLFWwindow* glfwWindow);
				void registerIFrameRoutes(RouterSim& router, CefRefPtr<UIHandler>& handler);
		};

	} 
} 
