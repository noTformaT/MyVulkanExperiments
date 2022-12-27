#pragma once

//#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <set>

#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();

	int Init(GLFWwindow* newWindow);
	void Cleanup();

	~VulkanRenderer();

private:
	GLFWwindow* window = nullptr;

	// Vulkan components
	VkInstance instance = nullptr;

	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;
	VkQueue presentationQueue;

	VkSurfaceKHR surface;

	// Vulkan functions
	// -Create functions
	void CreateInstance();
	void CreateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();

	// -Get functions
	void GetPhysicalDevice();

	// -Support functions
	// --Checker Functions
	bool CheckInstanceExtensionsSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceExtensionsSupport(VkPhysicalDevice device);
	bool CheckDeviceSuitable(VkPhysicalDevice device);

	// --Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device);

	// --Choose Formats
	VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentaionModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
};