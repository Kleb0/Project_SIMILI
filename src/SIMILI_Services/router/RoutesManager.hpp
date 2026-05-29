#pragma once

#include "RouterSim.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../SIMILI_Frontend/viewportLogic/UIPanels/FrameDataCatcher.hpp"

namespace SIMILI {
	namespace Frontend {
		class UIManager;
	}
}

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
					GLFWwindow* glfwWindow,
					FrameDataCatcher* frameCatcher,
					SIMILI::Frontend::UIManager* uiManager
				);

			private:
			void registerContextRoutes(RouterSim& router, VKContext& vkRenderer);
			void registerSceneRoutes(RouterSim& router, VKScene& scene, VKContext& vkRenderer);
			void registerObjectRoutes(RouterSim& router, VKScene& scene, GLFWwindow* glfwWindow);
			void registerIFrameRoutes(RouterSim& router, FrameDataCatcher* frameCatcher, SIMILI::Frontend::UIManager* uiManager);
		};

	} 
} 
