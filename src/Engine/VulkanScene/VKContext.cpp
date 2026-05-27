#include "VKcontext.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Camera/Camera.hpp"
#include "SIMILI_Frontend/viewportLogic/ThreeDScreen/ThreeDScreen.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

std::string VKContext::generateContextID()
{
	static const char alphanumeric[] = 
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, sizeof(alphanumeric) - 2);
	
	std::stringstream ss;
	ss << "VK-";
	
	for (int i = 0; i < 16; ++i) {
		ss << alphanumeric[dis(gen)];
	}
	
	return ss.str();
}

VKContext::VKContext()
	: instance(VK_NULL_HANDLE),
	  physicalDevice(VK_NULL_HANDLE),
	  device(VK_NULL_HANDLE),
	  graphicsQueue(VK_NULL_HANDLE),
	  presentQueue(VK_NULL_HANDLE),
	  surface_(VK_NULL_HANDLE),
	  debugMessenger_(VK_NULL_HANDLE),
	  graphicsQueueFamily(0),
	  presentQueueFamily(0)
{
	contextID = generateContextID();
	std::cout << "[VKContext] Created with ID: " << contextID << std::endl;
}

VKContext::~VKContext()
{
	std::cout << "[VKContext] Destroying context: " << contextID << std::endl;
	
	if (device != VK_NULL_HANDLE) {
		std::cout << "[VKContext] Waiting for device idle..." << std::endl;
		vkDeviceWaitIdle(device);
		std::cout << "[VKContext] Destroying device..." << std::endl;
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}
	if (surface_ != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
		std::cout << "[VKContext] Destroying surface..." << std::endl;
		vkDestroySurfaceKHR(instance, surface_, nullptr);
		surface_ = VK_NULL_HANDLE;
	}
	if (debugMessenger_ != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
		std::cout << "[VKContext] Destroying debug messenger..." << std::endl;
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger_, nullptr);
		}
		debugMessenger_ = VK_NULL_HANDLE;
	}
	if (instance != VK_NULL_HANDLE) {
		std::cout << "[VKContext] Destroying instance..." << std::endl;
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
	
	std::cout << "[VKContext] Context destroyed: " << contextID << std::endl;
}

void VKContext::initialize(SDL_Window* window)
{
	if (instance != VK_NULL_HANDLE) {
		std::cout << "[VKContext] Already initialized, skipping" << std::endl;
		return;
	}
	
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "SIMILI";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "SIMILI Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> extensions;
	if (window)
	{
		uint32_t sdlExtensionCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
		if (sdlExtensions)
		{
			for (uint32_t i = 0; i < sdlExtensionCount; ++i)
			{
				extensions.push_back(sdlExtensions[i]);
			}
			std::cout << "[VKContext] Loaded " << sdlExtensionCount << " SDL Vulkan extensions" << std::endl;
		}
	}

	// Add debug utils extension for validation layers
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	bool enableValidation = true;
	if (enableValidation)
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		bool validationAvailable = false;
		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(validationLayers[0], layerProperties.layerName) == 0)
			{
				validationAvailable = true;
				break;
			}
		}

		if (validationAvailable)
		{
			std::cout << "[VKContext] Enabling Vulkan validation layers for detailed error reporting" << std::endl;
		}
		else
		{
			std::cout << "[VKContext] WARNING: Validation layers not available" << std::endl;
			enableValidation = false;
		}
	}

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
	
	if (enableValidation)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		std::cerr << "[VKContext] Failed to create Vulkan instance. Result: " << result << std::endl;
		
		switch (result) {
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			std::cerr << "[VKContext] Error: Vulkan driver incompatible" << std::endl;
			break;
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			std::cerr << "[VKContext] Error: Required Vulkan extension not present" << std::endl;
			break;
		case VK_ERROR_LAYER_NOT_PRESENT:
			std::cerr << "[VKContext] Error: Required Vulkan layer not present" << std::endl;
			break;
		default:
			std::cerr << "[VKContext] Error: Unknown Vulkan initialization error" << std::endl;
			break;
		}
		return;
	}

	// Setup debug messenger to capture validation messages (logged via DualLogger)
	if (enableValidation)
	{
		auto debugCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		                        VkDebugUtilsMessageTypeFlagsEXT messageType,
		                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		                        void* pUserData) -> VkBool32
		{
			const char* severity = "UNKNOWN";
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				severity = "ERROR";
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				severity = "WARNING";
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
				severity = "INFO";

			const char* type = "GENERAL";
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
				type = "VALIDATION";
			else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				type = "PERFORMANCE";

			// Log to stdout (captured by DualLogger)
			std::cout << pCallbackData->pMessageIdName << "(" << severity << " / " << type << "): "
			          << "msgNum: " << pCallbackData->messageIdNumber << " - "
			          << pCallbackData->pMessage << std::endl;

			if (pCallbackData->objectCount > 0)
			{
				std::cout << "    Objects: " << pCallbackData->objectCount << std::endl;
				for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
				{
					std::cout << "        [" << i << "] 0x" << std::hex << pCallbackData->pObjects[i].objectHandle
					          << std::dec << ", type: " << pCallbackData->pObjects[i].objectType
					          << ", name: " << (pCallbackData->pObjects[i].pObjectName ? pCallbackData->pObjects[i].pObjectName : "NULL")
					          << std::endl;
				}
			}

			// Return VK_FALSE to prevent app termination
			return VK_FALSE;
		};

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			VkResult debugResult = func(instance, &debugCreateInfo, nullptr, &debugMessenger_);
			if (debugResult == VK_SUCCESS)
			{
				std::cout << "[VKContext] Debug messenger created successfully" << std::endl;
			}
			else
			{
				std::cout << "[VKContext] Failed to create debug messenger: " << debugResult << std::endl;
			}
		}
		else
		{
			std::cout << "[VKContext] vkCreateDebugUtilsMessengerEXT not available" << std::endl;
		}
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	
	if (deviceCount == 0) {
		std::cerr << "[VKContext] No Vulkan compatible devices found" << std::endl;
		return;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	physicalDevice = devices[0];

	if (window)
	{
		std::cout << "[VKContext] Creating Vulkan surface..." << std::endl;
		if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface_))
		{
			std::cerr << "[VKContext] Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
			return;
		}
		std::cout << "[VKContext] Vulkan surface created" << std::endl;
	}

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	bool foundQueue = false;
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueFamily = i;
			
			if (surface_ != VK_NULL_HANDLE)
			{
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface_, &presentSupport);
				if (presentSupport)
				{
					presentQueueFamily = i;
					foundQueue = true;
					std::cout << "[VKContext] Found queue family " << i << " supporting graphics and present" << std::endl;
					break;
				}
			}
			else
			{
				presentQueueFamily = i;
				foundQueue = true;
				break;
			}
		}
	}

	if (!foundQueue)
	{
		std::cerr << "[VKContext] Could not find suitable queue family" << std::endl;
		return;
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures supportedFeatures = {};
	vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

	// Enable only features actually supported by the selected GPU.
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.fillModeNonSolid = supportedFeatures.fillModeNonSolid;
	deviceFeatures.shaderClipDistance = supportedFeatures.shaderClipDistance;
	deviceFeatures.shaderCullDistance = supportedFeatures.shaderCullDistance;
	deviceFeatures.geometryShader = supportedFeatures.geometryShader;
	deviceFeatures.tessellationShader = supportedFeatures.tessellationShader;
	deviceFeatures.independentBlend = supportedFeatures.independentBlend;
	deviceFeatures.samplerAnisotropy = supportedFeatures.samplerAnisotropy;

	std::cout << "[VKContext] Device feature support:"
	          << " fillModeNonSolid=" << supportedFeatures.fillModeNonSolid
	          << " shaderClipDistance=" << supportedFeatures.shaderClipDistance
	          << " shaderCullDistance=" << supportedFeatures.shaderCullDistance
	          << " geometryShader=" << supportedFeatures.geometryShader
	          << " tessellationShader=" << supportedFeatures.tessellationShader
	          << " independentBlend=" << supportedFeatures.independentBlend
	          << " samplerAnisotropy=" << supportedFeatures.samplerAnisotropy
	          << std::endl;

	std::vector<const char*> deviceExtensions;
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
	if (result != VK_SUCCESS) {
		std::cerr << "[VKContext] Failed to create logical device" << std::endl;
		return;
	}

	vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);

	std::cout << "[VKContext] Vulkan initialized successfully" << std::endl;
}

void VKContext::resize(int w, int h)
{
	width = std::max(1, w);
	height = std::max(1, h);
	std::cout << "[VKContext] Resized to " << width << "x" << height << std::endl;
}

void VKContext::addObject(ThreeDObject* object)
{
	if (!object) return;
	objects.push_back(object);
}

bool VKContext::removeObject(ThreeDObject* object)
{
	if (!object) return false;
	auto it = std::find(objects.begin(), objects.end(), object);
	if (it != objects.end()) {
		objects.erase(it);
		return true;
	}
	return false;
}

void VKContext::pushInGraveyard(ThreeDObject* obj)
{
	if (!obj) return;
	
	if (removeObject(obj)) {
		graveyard.push_back(obj);
		std::cout << "[VKContext] Object moved to graveyard: " << obj->getID() << std::endl;
	}
}

bool VKContext::containsObject(const ThreeDObject* obj) const
{
	return std::find(objects.begin(), objects.end(), obj) != objects.end();
}

void VKContext::setCamera(Camera* cam)
{
	camera_ = cam;
}

