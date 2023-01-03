#pragma once

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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