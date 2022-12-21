#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

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

	// Vulkan functions
	// -Create functions
	void CreateInstance();
	void CreateLogicalDevice();

	// -Get functions
	void GetPhysicalDevice();

	// -Support functions
	// --Checker Functions
	bool CheckInstanceExtensionsSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceSuitable(VkPhysicalDevice device);

	// --Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
};