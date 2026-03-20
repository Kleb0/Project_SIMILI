#include "VKcontext.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Camera/Camera.hpp"
#include "SIMILI_Frontend/viewportLogic/ThreeDScreen/ThreeDScreen.hpp"
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
	  graphicsQueueFamily(0),
	  presentQueueFamily(0)
{
	contextID = generateContextID();
	std::cout << "[VKContext] Created with ID: " << contextID << std::endl;
}

VKContext::~VKContext()
{
	if (device != VK_NULL_HANDLE) {
		vkDestroyDevice(device, nullptr);
	}
	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
	}
}

void VKContext::initialize()
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

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		std::cerr << "[VKContext] Failed to create Vulkan instance" << std::endl;
		return;
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

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueFamily = i;
			presentQueueFamily = i;
			break;
		}
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

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

void VKContext::ProjectOnThreeDScreen()
{
	if (!camera_)
	{
		std::cout << "[VKContext] ProjectOnThreeDScreen: No camera set" << std::endl;
		return;
	}
	
	if (!three_d_screen_)
	{
		std::cout << "[VKContext] ProjectOnThreeDScreen: No ThreeDScreen set" << std::endl;
		return;
	}
	
	if (!three_d_screen_->hasValidViewport())
	{
		return;
	}
	
	camera_->ProjectScene(three_d_screen_);
}
