#pragma once

#include <fstream>

#include "glm/glm.hpp"

const int MAX_FRAME_DRAWS = 3;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex
{
	glm::vec3 vertexPosition;			// Vertex position (x, y, z)
	glm::vec3 vertexColor;				// Vertex color (r, g, b)
};

// Indieces (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;			// Locations of Graphics Queue Family
	int presentationFamily = -1;		// Location of Presentation Queue Family

	// Check if queue family is valid
	bool IsValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}

};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;		// Surface properies, e. g. image size, extent...
	std::vector<VkSurfaceFormatKHR> formats;			// Surface image formats, e. g. RGBA and size of each colout
	std::vector<VkPresentModeKHR> presentationModes;	// How images should be presented to screen

	// Valid if formats > 0 and presentations modes > 0
	bool IsValid()
	{
		return !formats.empty() && !presentationModes.empty();
	}
};

// SwapChain image struct
struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& fileName)
{
	// open stream from given file
	// std::ios::binary tells stream to read file as binary
	// std::ios::ate tells stream to start reading from end of file
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	printf("Read binary file: %s\n", fileName.c_str());

	// Check if file stream successfully open
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file!");
	}

	// Get current read position to resize file buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	// Move read position (seek to) the start of the file
	file.seekg(0);

	// Read the file data into the buffer (stream "fileSize" in total)
	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}