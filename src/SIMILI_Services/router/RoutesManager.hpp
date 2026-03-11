#pragma once

#include "RouterSim.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../SIMILI_Frontend/ui_handler.hpp"

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
					VKContext& vkRenderer,
					VKScene& scene,
					CefRefPtr<UIHandler>& handler,
					GLFWwindow* glfwWindow
				);

			private:
			void registerContextRoutes(RouterSim& router, VKContext& vkRenderer);
			void registerSceneRoutes(RouterSim& router, VKScene& scene, VKContext& vkRenderer);
			void registerObjectRoutes(RouterSim& router, VKScene& scene, CefRefPtr<UIHandler>& handler, GLFWwindow* glfwWindow);
			void registerIFrameRoutes(RouterSim& router, CefRefPtr<UIHandler>& handler);
		};

	} 
} 
