#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::Init(GLFWwindow* newWindow)
{
	window = newWindow;

	try
	{
		CreateInstance();
		CreateSurface();
		GetPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateGraphicsPipeline();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
	}

	return 0;
}

void VulkanRenderer::Cleanup()
{
	for (SwapChainImage& image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapChain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::CreateInstance()
{
	// Information about application itself
	// Most data there doesn't affect programm and is for developer convience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;				
	appInfo.pApplicationName = "Vulkan App";						// Custom name of application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);			// Custom version of application
	appInfo.pEngineName = "No Engine";								// Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);				// Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_0;						// The Vulkan vertion

	// Creation information for VKInstance (Vulkan Instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	
	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	// Set up extensions Instances will use
	uint32_t glfwExtensionCount = 0;	// GLFW may require multiplr extensions
	const char **glfwExtensions;		// Extensions passed as array of cstrings, so need pointer (the array) to pointer (char)

	// Get GLFW extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	// Check instance Extensions suported...
	if (!CheckInstanceExtensionsSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstane does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// TODO: Set up validation Layers that Vulkan Instance will use
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = nullptr;

	// Create Instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create ");
	}
}

void VulkanRenderer::CreateLogicalDevice()
{
	// Get queue family indices fro the chosen Physical Device
	QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentationFamily};

	// Queues the logical device needs to create and info to do so (Only 1 for now, will add more later!)
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;				// The index of the family to create a queue from
		queueCreateInfo.queueCount = 1;										// Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;						// Vulkan need to know how to handle multiple queues, so decide priority (1 = highest priority)
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Information to create logical device (some called device)
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());		// Number of Queue Create Infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();								// List of queue infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());	// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();							// List of enabled logical device extensions
	
	// Physical Device Features the Logical Device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;	// Physical Device features Logical Device will use
	
	// Create the logival device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Logical Device!");
	}

	// Queues are created at the same time as the device...
	// So we want handle to qeueus
	// From given logical device, of given Queue Index (0 since only one queue), place reference in given vkQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::CreateSurface()
{
	// Create Surface (creates a surface create info struct, runs the create surface function, returns result)
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create surface!");
	}
}

void VulkanRenderer::CreateSwapChain()
{
	// Get Swap Chain details so we can pick best settings
	SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

	// Find optimal surface values for our swap chain

	// Choose best surface format
	VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapChainDetails.formats);

	// Choose best presentation format
	VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);

	// Choose Swap Chain image resolution
	VkExtent2D extent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swap chain? Get 1 more that the minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// If image count higher than max, then clamp down to max
	// If 0, then limitless
	if (swapChainDetails.surfaceCapabilities.minImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	// Creating information for swap chain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.surface = surface;										// Swapchain surface
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;	// Swapchain structure
	swapChainCreateInfo.imageFormat = surfaceFormat.format;						// Swapchain format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;				// Swapchain color space
	swapChainCreateInfo.presentMode = presentMode;								// Swapchain present mode
	swapChainCreateInfo.imageExtent = extent;									// Swapchain extent
	swapChainCreateInfo.minImageCount = imageCount;								// Swapchain image count
	swapChainCreateInfo.imageArrayLayers = 1;									// Number of layer for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;		// What attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform; // Transform to perform on swap chain
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending images with external graphics (e. g. other windows)
	swapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g. behind another window, off screen, etc)

	// Get Queue Family indices
	QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

	// If graphics and Presentation families are different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;	// Image share handling 
		swapChainCreateInfo.queueFamilyIndexCount = 2;						// Number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;		// Array of queues to share between
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}
	// If old swapchain been destroyed and this one replaces it, then link old one to quikly hand over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create Swapchain

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Swapchain!");
	}
	
	// Store for later reference
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// Get Swapchain images (first count, then values)
	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapchainImageCount, nullptr);
	std::vector<VkImage> images(swapchainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapchainImageCount, images.data());

	for (VkImage image : images)
	{
		// Store image hangle
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;

		// Create image view here
		swapChainImage.imageView = CreateImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::CreateGraphicsPipeline()
{
	printf("STAGE: Create Graphics Pipeline\n\n");

	// -- SHADER MODULES --
	
	// Read in SPIR-V code in shaders
	printf("Load Vertex shader SPIR-V code\n");
	auto vertexShaderCode = readFile("../Shaders/vert.spv");
	printf("Load Fragment shader SPIR-V code\n");
	auto fragmentShaderCode = readFile("../Shaders/frag.spv");

	// Create shader modules
	printf("Create Vertex shader module\n");
	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
	printf("Create Fragment shader module\n");
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

	// Vertex Stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;							// Shader stage name
	vertexShaderCreateInfo.module = vertexShaderModule;									// Shader module to be used by stage
	vertexShaderCreateInfo.pName = "main";												// Entry point in to shader
																			// Fragment Stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;						// Shader stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;								// Shader module to be used by stage
	fragmentShaderCreateInfo.pName = "main";											// Entry point in to shader

	// Put shader stage creation info in to array
	// Graphics Pipeline creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	
	// -- VERTEX INPUT -- (TODO: Put in vertex descriptions when resources created)
	printf("Create Vertex input state\n");
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;			// List of vertex binding descriptors (data spacing/stride infos)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;		// List of vertex attribute descriptors (data format and where to bind to/from)
		
	
	// -- INPUT ASSEMBLY -- 
	printf("Create Pipeline input assembly state\n");
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// Primitive type to assemble vertices as
	inputAssembly.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitive

	
	// -- VIEWPORT AND SCISSOR --
	VkViewport viewPort = {};
	viewPort.x = 0.0f;									// x start coordinate
	viewPort.y = 0.0f;									// y start coordinage
	viewPort.width = (float)swapChainExtent.width;		// width view port
	viewPort.height = (float)swapChainExtent.height;	// height view port
	viewPort.minDepth = 0.0f;							// min framebuffer depth
	viewPort.maxDepth = 1.0f;							// max framebuffer depth
	printf("Create Viewport with { offset_x: %.2f, offset_y: %.2f, width: %.2f, height: %.2f, minDepth: %.2f, maxDepth: %.2f }\n", viewPort.x, viewPort.y, viewPort.width, viewPort.height, viewPort.minDepth, viewPort.maxDepth);

	// Create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };							// Offset to use region from
	scissor.extent = { swapChainExtent };				// Extent to descripe region to use
	printf("Create Scissor with { offset_x: %.2f, offset_y: %.2f, width: %.2f, height: %.2f }\n", (float)scissor.offset.x, (float)scissor.offset.y, (float)scissor.extent.width, (float)scissor.extent.height);

	VkPipelineViewportStateCreateInfo viewPortStateCreateInfo = {};
	viewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewPortStateCreateInfo.viewportCount = 1;
	viewPortStateCreateInfo.pViewports = &viewPort;
	viewPortStateCreateInfo.scissorCount = 1;
	viewPortStateCreateInfo.pScissors = &scissor;

	// -- DYNAMIC STATES --
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);		// Dynamic viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);		// Dynamic scissor  : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	// Dynamic State creation info
	printf("Create Pipeline Dynamic States\n");
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;

	// Destroy shader modules, no longer needed after Pipeline created
	printf("Destroy Vertex shader module\n");
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	printf("Destroy Fragment shader module\n");
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

	printf("----------------------------------\n");
}

void VulkanRenderer::GetPhysicalDevice()
{
	// Enumerate physical devies the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// Throw if no device avaible, then none support Vulkan!
	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPUs that support Vulan Instance!");
	}

	// Get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}
}

bool VulkanRenderer::CheckInstanceExtensionsSupport(std::vector<const char*>* checkExtensions)
{
	// Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create a list of VKEsxtensionProperties using cout
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// Check of given extensions are in list of avaible extensions
	for (const auto& checkExtension : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName))
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckDeviceExtensionsSupport(VkPhysicalDevice device)
{
	// Get device extension count
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	// If no extensions found, return failure
	if (extensionCount == 0)
	{
		return false;
	}

	// Populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	// Check for extension
	for (const auto &deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName))
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device)
{
	// Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Information about what the device can do(geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = GetQueueFamilies(device);

	bool extensionSupported = CheckDeviceExtensionsSupport(device);

	bool swapChainValid = false;

	if (extensionSupported)
	{
		SwapChainDetails swapChain = GetSwapChainDetails(device);
		swapChainValid = swapChain.IsValid();
	}

	return indices.IsValid() && extensionSupported && swapChainValid;
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	// Get all Queue Family Property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// Go throuth each queue family and chek if it has at least 1 of the required types of queue
	int i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		// First check if queue family has a least 1 queue in that family (could have no queues)
		// Queue can be multiple types defined through bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to check if has required type
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i; // if queue family is valid, then get index;
		}

		// Check id Queue Family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		// Check if queue is presentation type (can be both graphics and presention)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		// Check if queue family indices are in valid state, stop searchable is so
		if (indices.IsValid())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// Get surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// Get surface formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	// If formats returned, get list of formats
	if (formatCount > 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	// Get presentation modes
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	if (presentationCount > 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

// Best format is subjective, but ours will be:
// Format		: VK_FORMAT_R8G8B8A8_UNORM (VK_FORMAT_R8G8B8A8_UNORM as buckup)
// ColorSpace	: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// If only 1 format avaible and is undefined, then this means all formats are avaible (no restrictions)
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// If restricted, search for optimal format
	for (const auto& format : formats)
	{
		if( (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	// If can`t find optimal format, then just return first format
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	// look for Mailbox presentation mode
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	// If can`t find, use FIFO as Vulkan spec
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	// If current extent is at numeric limits, then extent can vary. Îtherwise, it is the size of the window
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		int width;
		int height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create new extent usign window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also define max and min, so make sure within boundaries by clamping value
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;										// Image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					// Type of image (1D, 2D, 3D, Cube, etc)
	viewCreateInfo.format = format;										// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;		// Allows remapping of RGBA components to other RGBA value
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	
	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;			// Which aspect of images to view (e.g. COLOR_BIT to view color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;					// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;						// Number of mip map levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;					// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 0;						// Number of array levels to view

	// Create Image View
	VkImageView imageView;
	
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image View!");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code)
{
	// Shader Module creation information
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();										// Size of code
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());		// Pointer to code (of uint32_t pointer type)

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Shader Module");
	}

	return shaderModule;
}
