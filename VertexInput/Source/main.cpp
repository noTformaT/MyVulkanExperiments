#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>

#include "VulkanRenderer.h"

GLFWwindow* window = nullptr;
VulkanRenderer vulkanRenderer;

void initWindow(std::string wName, const int width = 800, const int height = 600)
{
	// Initialize GLFW
	glfwInit();

	// Set GLFW not work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main()
{
	// Create window
	initWindow("My Vulkan app");

	// Create Vulkan Renderer instance
	if (vulkanRenderer.Init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	// Loop until close
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		vulkanRenderer.Draw();
	}

	// Clear Vulkan
	vulkanRenderer.Cleanup();

	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}