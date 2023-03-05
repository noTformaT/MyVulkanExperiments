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
		CreateDebugCallback();
		CreateSurface();
		GetPhysicalDevice();
		CreateLogicalDevice();

		// Create a mesh
		std::vector<Vertex> vertices{
			{{ 0.4f,-0.4f, 0.0f}, { 1.0f, 0.0f, 0.0f}},
			{{ 0.4f, 0.4f, 0.0f}, { 0.0f, 1.0f, 0.0f}},
			{{-0.4f, 0.4f, 0.0f}, { 0.0f, 0.0f, 1.0f}},

			{{-0.4f, 0.4f, 0.0}, { 0.0f, 0.0f, 1.0f}},
			{{-0.4f,-0.4f, 0.0}, { 0.0f, 1.0f, 0.0f}},
			{{ 0.4f,-0.4f, 0.0}, { 1.0f, 0.0f, 0.0f}},
		};

		firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, &vertices);

		CreateSwapChain();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFrameBuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		RecordCommands();
		CreateSynchronisation();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::Draw()
{
	// 1. Get next available image to draw to and set something to signal when we're finished with the image (a semaphore)
	// 2. Submit CommandBuffer to queue for execution, making sure it wait for the image to be signalled as available before drawing
	// and signals when it has finished rendering
	// 3. Present image to screen when it has signalled finished rendering

	// -- GET NEXT IMAGE --
	
	// Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits< uint64_t>::max());
	// Manualy reset (close) fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// Get index of next image to be draw to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(mainDevice.logicalDevice, swapChain, std::numeric_limits<uint64_t>::max(), imageAvaible[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Fuck!");
	}
	//printf("imageIndex = %u \n", imageIndex);

	// -- SUBMIT COMMAND BUFFER TO RENDER --
	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;								// Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &imageAvaible[currentFrame];		// List of semaphores to wait on
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;						// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;								// Number of command buffer submited
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];		// Command Buffer to sumbit
	submitInfo.signalSemaphoreCount = 1;							// Number of semaphores to signal
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];	// Semaphores to signal when command buffer finishes
	
	// Submit command buffer to queue
	result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue!");
	}

	// -- PRESENT RENDERED IMAGE TO SCREEN --
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;							// Number of semaphores to wait on
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];// List of semaphores to wait on
	presentInfo.swapchainCount = 1;								// Number of swapchains to present to
	presentInfo.pSwapchains = &swapChain;						// Swapchains to present images to
	presentInfo.pImageIndices = &imageIndex;					// Index of images in swapchains to present

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present Image!");
	}

	// Get next frame (use % MAX_FRAME_DRAWS to keep value below MAX_FRAME_DRAWS)
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::Cleanup()
{
	// Wait until no actions being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	firstMesh.DestroyVertexBuffer();

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvaible[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (VkFramebuffer& frameBuffer : swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, frameBuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (SwapChainImage& image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapChain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	if (validationEnabled)
	{
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::CreateInstance()
{
	printf("----------------------------------\n");
	printf("My Vulkan Render...\nAuthor: Eugene Karpenko\n");
	printf("----------------------------------\n");

	if (validationEnabled && !CheckValidationLayerSupport())
	{
		std::runtime_error("Required Validation Layers not supported!");
	}

	printf("STAGE: Create Vulkan Instance \n\n");

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
	printf("Vulkan extensions count: %u\n", glfwExtensionCount);

	// Add GLFW extensions to list of extensions
	printf("Vulkan extensions list:\n");
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
		printf("\t%s\n", glfwExtensions[i]);
	}

	// If validation enabled, add extension to report validation debug info
	if (validationEnabled)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	// Check instance Extensions suported...
	if (!CheckInstanceExtensionsSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstane does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Set up validation Layers that Vulkan Instance will use
	if (validationEnabled)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	// Create Instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create ");
	}
	printf("Vulkan Instance create successful\n");
	printf("----------------------------------\n");
}

void VulkanRenderer::CreateDebugCallback()
{
	if (!validationEnabled)
	{
		return;
	}

	VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
	callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;	// Which validation reports should initiate callback
	callbackCreateInfo.pfnCallback = debugCallback;												// Pointer to callback function itself

	// Create debug callback with custom create function
	VkResult result = CreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Debug Callback!");
	}
}

void VulkanRenderer::CreateLogicalDevice()
{
	printf("STAGE: Create Vulkan Logical Device \n\n");

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
	printf("Logical Device successful connect to Physical Device\n");
	printf("----------------------------------\n");
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

inline std::string presentModeKHRString(const VkPresentModeKHR presentMode)
{
	switch (presentMode)
	{
#define STR(r) case VK_PRESENT_MODE_##r##_KHR: return #r
		STR(IMMEDIATE);
		STR(MAILBOX);
		STR(FIFO);
		STR(FIFO_RELAXED);
		STR(SHARED_DEMAND_REFRESH);
		STR(SHARED_CONTINUOUS_REFRESH);
#undef STR
	default: return "UNKNOWN_ENUM";
	}
}

inline std::string formatString(const VkFormat format)
{
	switch (format)
	{
#define STR(r) case VK_FORMAT_##r: return #r
		STR(UNDEFINED);
		STR(R4G4_UNORM_PACK8);
		STR(R4G4B4A4_UNORM_PACK16);
		STR(B4G4R4A4_UNORM_PACK16);
		STR(R5G6B5_UNORM_PACK16);
		STR(B5G6R5_UNORM_PACK16);
		STR(R5G5B5A1_UNORM_PACK16);
		STR(B5G5R5A1_UNORM_PACK16);
		STR(A1R5G5B5_UNORM_PACK16);
		STR(R8_UNORM);
		STR(R8_SNORM);
		STR(R8_USCALED);
		STR(R8_SSCALED);
		STR(R8_UINT);
		STR(R8_SINT);
		STR(R8_SRGB);
		STR(R8G8_UNORM);
		STR(R8G8_SNORM);
		STR(R8G8_USCALED);
		STR(R8G8_SSCALED);
		STR(R8G8_UINT);
		STR(R8G8_SINT);
		STR(R8G8_SRGB);
		STR(R8G8B8_UNORM);
		STR(R8G8B8_SNORM);
		STR(R8G8B8_USCALED);
		STR(R8G8B8_SSCALED);
		STR(R8G8B8_UINT);
		STR(R8G8B8_SINT);
		STR(R8G8B8_SRGB);
		STR(B8G8R8_UNORM);
		STR(B8G8R8_SNORM);
		STR(B8G8R8_USCALED);
		STR(B8G8R8_SSCALED);
		STR(B8G8R8_UINT);
		STR(B8G8R8_SINT);
		STR(B8G8R8_SRGB);
		STR(R8G8B8A8_UNORM);
		STR(R8G8B8A8_SNORM);
		STR(R8G8B8A8_USCALED);
		STR(R8G8B8A8_SSCALED);
		STR(R8G8B8A8_UINT);
		STR(R8G8B8A8_SINT);
		STR(R8G8B8A8_SRGB);
		STR(B8G8R8A8_UNORM);
		STR(B8G8R8A8_SNORM);
		STR(B8G8R8A8_USCALED);
		STR(B8G8R8A8_SSCALED);
		STR(B8G8R8A8_UINT);
		STR(B8G8R8A8_SINT);
		STR(B8G8R8A8_SRGB);
		STR(A8B8G8R8_UNORM_PACK32);
		STR(A8B8G8R8_SNORM_PACK32);
		STR(A8B8G8R8_USCALED_PACK32);
		STR(A8B8G8R8_SSCALED_PACK32);
		STR(A8B8G8R8_UINT_PACK32);
		STR(A8B8G8R8_SINT_PACK32);
		STR(A8B8G8R8_SRGB_PACK32);
		STR(A2R10G10B10_UNORM_PACK32);
		STR(A2R10G10B10_SNORM_PACK32);
		STR(A2R10G10B10_USCALED_PACK32);
		STR(A2R10G10B10_SSCALED_PACK32);
		STR(A2R10G10B10_UINT_PACK32);
		STR(A2R10G10B10_SINT_PACK32);
		STR(A2B10G10R10_UNORM_PACK32);
		STR(A2B10G10R10_SNORM_PACK32);
		STR(A2B10G10R10_USCALED_PACK32);
		STR(A2B10G10R10_SSCALED_PACK32);
		STR(A2B10G10R10_UINT_PACK32);
		STR(A2B10G10R10_SINT_PACK32);
		STR(R16_UNORM);
		STR(R16_SNORM);
		STR(R16_USCALED);
		STR(R16_SSCALED);
		STR(R16_UINT);
		STR(R16_SINT);
		STR(R16_SFLOAT);
		STR(R16G16_UNORM);
		STR(R16G16_SNORM);
		STR(R16G16_USCALED);
		STR(R16G16_SSCALED);
		STR(R16G16_UINT);
		STR(R16G16_SINT);
		STR(R16G16_SFLOAT);
		STR(R16G16B16_UNORM);
		STR(R16G16B16_SNORM);
		STR(R16G16B16_USCALED);
		STR(R16G16B16_SSCALED);
		STR(R16G16B16_UINT);
		STR(R16G16B16_SINT);
		STR(R16G16B16_SFLOAT);
		STR(R16G16B16A16_UNORM);
		STR(R16G16B16A16_SNORM);
		STR(R16G16B16A16_USCALED);
		STR(R16G16B16A16_SSCALED);
		STR(R16G16B16A16_UINT);
		STR(R16G16B16A16_SINT);
		STR(R16G16B16A16_SFLOAT);
		STR(R32_UINT);
		STR(R32_SINT);
		STR(R32_SFLOAT);
		STR(R32G32_UINT);
		STR(R32G32_SINT);
		STR(R32G32_SFLOAT);
		STR(R32G32B32_UINT);
		STR(R32G32B32_SINT);
		STR(R32G32B32_SFLOAT);
		STR(R32G32B32A32_UINT);
		STR(R32G32B32A32_SINT);
		STR(R32G32B32A32_SFLOAT);
		STR(R64_UINT);
		STR(R64_SINT);
		STR(R64_SFLOAT);
		STR(R64G64_UINT);
		STR(R64G64_SINT);
		STR(R64G64_SFLOAT);
		STR(R64G64B64_UINT);
		STR(R64G64B64_SINT);
		STR(R64G64B64_SFLOAT);
		STR(R64G64B64A64_UINT);
		STR(R64G64B64A64_SINT);
		STR(R64G64B64A64_SFLOAT);
		STR(B10G11R11_UFLOAT_PACK32);
		STR(E5B9G9R9_UFLOAT_PACK32);
		STR(D16_UNORM);
		STR(X8_D24_UNORM_PACK32);
		STR(D32_SFLOAT);
		STR(S8_UINT);
		STR(D16_UNORM_S8_UINT);
		STR(D24_UNORM_S8_UINT);
		STR(D32_SFLOAT_S8_UINT);
		STR(BC1_RGB_UNORM_BLOCK);
		STR(BC1_RGB_SRGB_BLOCK);
		STR(BC1_RGBA_UNORM_BLOCK);
		STR(BC1_RGBA_SRGB_BLOCK);
		STR(BC2_UNORM_BLOCK);
		STR(BC2_SRGB_BLOCK);
		STR(BC3_UNORM_BLOCK);
		STR(BC3_SRGB_BLOCK);
		STR(BC4_UNORM_BLOCK);
		STR(BC4_SNORM_BLOCK);
		STR(BC5_UNORM_BLOCK);
		STR(BC5_SNORM_BLOCK);
		STR(BC6H_UFLOAT_BLOCK);
		STR(BC6H_SFLOAT_BLOCK);
		STR(BC7_UNORM_BLOCK);
		STR(BC7_SRGB_BLOCK);
		STR(ETC2_R8G8B8_UNORM_BLOCK);
		STR(ETC2_R8G8B8_SRGB_BLOCK);
		STR(ETC2_R8G8B8A1_UNORM_BLOCK);
		STR(ETC2_R8G8B8A1_SRGB_BLOCK);
		STR(ETC2_R8G8B8A8_UNORM_BLOCK);
		STR(ETC2_R8G8B8A8_SRGB_BLOCK);
		STR(EAC_R11_UNORM_BLOCK);
		STR(EAC_R11_SNORM_BLOCK);
		STR(EAC_R11G11_UNORM_BLOCK);
		STR(EAC_R11G11_SNORM_BLOCK);
		STR(ASTC_4x4_UNORM_BLOCK);
		STR(ASTC_4x4_SRGB_BLOCK);
		STR(ASTC_5x4_UNORM_BLOCK);
		STR(ASTC_5x4_SRGB_BLOCK);
		STR(ASTC_5x5_UNORM_BLOCK);
		STR(ASTC_5x5_SRGB_BLOCK);
		STR(ASTC_6x5_UNORM_BLOCK);
		STR(ASTC_6x5_SRGB_BLOCK);
		STR(ASTC_6x6_UNORM_BLOCK);
		STR(ASTC_6x6_SRGB_BLOCK);
		STR(ASTC_8x5_UNORM_BLOCK);
		STR(ASTC_8x5_SRGB_BLOCK);
		STR(ASTC_8x6_UNORM_BLOCK);
		STR(ASTC_8x6_SRGB_BLOCK);
		STR(ASTC_8x8_UNORM_BLOCK);
		STR(ASTC_8x8_SRGB_BLOCK);
		STR(ASTC_10x5_UNORM_BLOCK);
		STR(ASTC_10x5_SRGB_BLOCK);
		STR(ASTC_10x6_UNORM_BLOCK);
		STR(ASTC_10x6_SRGB_BLOCK);
		STR(ASTC_10x8_UNORM_BLOCK);
		STR(ASTC_10x8_SRGB_BLOCK);
		STR(ASTC_10x10_UNORM_BLOCK);
		STR(ASTC_10x10_SRGB_BLOCK);
		STR(ASTC_12x10_UNORM_BLOCK);
		STR(ASTC_12x10_SRGB_BLOCK);
		STR(ASTC_12x12_UNORM_BLOCK);
		STR(ASTC_12x12_SRGB_BLOCK);
		STR(G8B8G8R8_422_UNORM);
		STR(B8G8R8G8_422_UNORM);
		STR(G8_B8_R8_3PLANE_420_UNORM);
		STR(G8_B8R8_2PLANE_420_UNORM);
		STR(G8_B8_R8_3PLANE_422_UNORM);
		STR(G8_B8R8_2PLANE_422_UNORM);
		STR(G8_B8_R8_3PLANE_444_UNORM);
		STR(R10X6_UNORM_PACK16);
		STR(R10X6G10X6_UNORM_2PACK16);
		STR(R10X6G10X6B10X6A10X6_UNORM_4PACK16);
		STR(G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
		STR(B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
		STR(G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
		STR(G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
		STR(G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
		STR(G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
		STR(G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
		STR(R12X4_UNORM_PACK16);
		STR(R12X4G12X4_UNORM_2PACK16);
		STR(R12X4G12X4B12X4A12X4_UNORM_4PACK16);
		STR(G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
		STR(B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
		STR(G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
		STR(G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
		STR(G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
		STR(G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
		STR(G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
		STR(G16B16G16R16_422_UNORM);
		STR(B16G16R16G16_422_UNORM);
		STR(G16_B16_R16_3PLANE_420_UNORM);
		STR(G16_B16R16_2PLANE_420_UNORM);
		STR(G16_B16_R16_3PLANE_422_UNORM);
		STR(G16_B16R16_2PLANE_422_UNORM);
		STR(G16_B16_R16_3PLANE_444_UNORM);
		STR(PVRTC1_2BPP_UNORM_BLOCK_IMG);
		STR(PVRTC1_4BPP_UNORM_BLOCK_IMG);
		STR(PVRTC2_2BPP_UNORM_BLOCK_IMG);
		STR(PVRTC2_4BPP_UNORM_BLOCK_IMG);
		STR(PVRTC1_2BPP_SRGB_BLOCK_IMG);
		STR(PVRTC1_4BPP_SRGB_BLOCK_IMG);
		STR(PVRTC2_2BPP_SRGB_BLOCK_IMG);
		STR(PVRTC2_4BPP_SRGB_BLOCK_IMG);
#undef STR
	default: return "UNKNOWN_ENUM";
	}
}

inline std::string colorSpaceKHRString(const VkColorSpaceKHR colorSpace)
{
	switch (colorSpace)
	{
#define STR(r) case VK_COLOR_SPACE_##r: return #r
		STR(SRGB_NONLINEAR_KHR);
		STR(DISPLAY_P3_NONLINEAR_EXT);
		STR(EXTENDED_SRGB_LINEAR_EXT);
		STR(DISPLAY_P3_LINEAR_EXT);
		STR(DCI_P3_NONLINEAR_EXT);
		STR(BT709_LINEAR_EXT);
		STR(BT709_NONLINEAR_EXT);
		STR(BT2020_LINEAR_EXT);
		STR(HDR10_ST2084_EXT);
		STR(DOLBYVISION_EXT);
		STR(HDR10_HLG_EXT);
		STR(ADOBERGB_LINEAR_EXT);
		STR(ADOBERGB_NONLINEAR_EXT);
		STR(PASS_THROUGH_EXT);
		STR(EXTENDED_SRGB_NONLINEAR_EXT);
		STR(DISPLAY_NATIVE_AMD);
#undef STR
	default: return "UNKNOWN_ENUM";
	}
}


void VulkanRenderer::CreateSwapChain()
{
	printf("STAGE: Create Swap Chain\n\n");
	// Get Swap Chain details so we can pick best settings
	SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

	// Find optimal surface values for our swap chain

	// Choose best surface format
	VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapChainDetails.formats);
	printf("Surface format: %s\n", formatString(surfaceFormat.format).c_str());
	printf("Surface color space: %s\n", colorSpaceKHRString(surfaceFormat.colorSpace).c_str());

	// Choose best presentation format
	VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);
	printf("Surface presentaion mode: %s\n", presentModeKHRString(presentMode).c_str());

	// Choose Swap Chain image resolution
	VkExtent2D extent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);
	printf("Surface width: %i\n", extent.width);
	printf("Surface height: %i\n", extent.height);

	// How many images are in the swap chain? Get 1 more that the minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// If image count higher than max, then clamp down to max
	// If 0, then limitless
	if (swapChainDetails.surfaceCapabilities.minImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	printf("Surface count: %i\n", imageCount);

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
	printf("Swap Chain with %i Surface successful created\n", imageCount);
	printf("----------------------------------\n");
}

void VulkanRenderer::CreateRenderPass()
{
	printf("STAGE: Create Render Pass\n\n");
	// Colour attachments of render pass
	VkAttachmentDescription colourAttachment = {};
	colourAttachment.format = swapChainImageFormat;						// Format to use for attachment
	colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					// Number of samples to write for multisampling
	colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// Describe what to do with attachment before rendering
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;			// Describe what to do with attachment affter rendering
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Describes what to do with stencil before rendering
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// Describes what to do with stencil after rendering

	// Framebuffer data will be store as an image, but image can given difference data layouts
	// to give optimal use for certain operations
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			// Image data layout before render pass starts
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// Image data layout affter render pass (to change to)


	// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference colourAttachmentReference = {};
	colourAttachmentReference.attachment = 0;
	colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
	// Information about a particular subpass the Render Pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;		// Pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachmentReference;

	// Need to determine when layout transitions occur using subpass dependenciess
	std::array<VkSubpassDependency, 2> subpassDependencies;

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;						// Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;		// Pipeline stage 
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;				// Stage access mask (memory access)
	// But must happen before...
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL to VK_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies[1].srcSubpass = 0;											// Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;		// Pipeline stage 
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// But must happen before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	// Create info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colourAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Render Pass!");
	}

	printf("Render Pass successful created\n");
	printf("----------------------------------\n");
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

	
	// How the data for a single vertex (including info such as position, colour, texture coords, normals, etc) is a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;										// Can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);							// Size of single object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;			// How to move vertex date after each vertex.
																		// VK_VERTEX_INPUT_RATE_VERTEX		: Move on the next vertex
																		// VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex the next instance

	// How the data for a attribute	is defined with a vertex
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
	
	// Position Atribute
	attributeDescriptions[0].binding = 0;								// Which binding the data is at (should be same as above)
	attributeDescriptions[0].location = 0;								// Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define size of data)
	attributeDescriptions[0].offset = offsetof(Vertex, vertexPosition);	// Where this attribute is defined in the data for a single vertex

	// Colour Attribute: TODO
	attributeDescriptions[1].binding = 0;								// Which binding the data is at (should be same as above)
	attributeDescriptions[1].location = 1;								// Location in shader where data will be read from
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define size of data)
	attributeDescriptions[1].offset = offsetof(Vertex, vertexColor);	// Where this attribute is defined in the data for a single vertex

	// -- VERTEX INPUT --
	printf("Create Vertex input state\n");
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;											// List of vertex binding descriptors (data spacing/stride infos)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();								// List of vertex attribute descriptors (data format and where to bind to/from)
		
	
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
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);		// Dynamic viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);		// Dynamic scissor  : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	// Dynamic State creation info
	printf("Create Dynamic States\n");
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	// -- RASTERIZER --
	printf("Create Rasterization State\n");
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;			// Change if fragment beyond near/far planes are clipped (default) or clamped to plane
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	// Whether to discard data and skip resterizer. Never creates fragment, only suitable for pipeline without framebuffer output
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	// How to handle filling points between vertices
	rasterizationStateCreateInfo.lineWidth = 1.0f;						// How thick line should be when drawn
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;		// Which face of a tri to cull
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;	// Winding to determine which side is front
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;			// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

	// -- MULTISAMPLING --
	printf("Create Multisampling State\n");
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;					// Enable multisample shading or not
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// Number of samples to use per fragment

	// -- BLENDING --
	// Blending decides how to blend a new color being a fragment, with the old value
	
	// Blend Attachment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT		// Colors to apply blending to 
		| VK_COLOR_COMPONENT_G_BIT 
		| VK_COLOR_COMPONENT_B_BIT 
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentState.blendEnable = VK_TRUE;						// Enable blending

	// Blending uses equation: (srcColorBlendFactor * new color) colorBlendOp(destinationBlendFactor * old Color)
	// Summarised:	(VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)
	//				(new color alpha * new color) + ((1 - new color alpha) * old color)
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	
	// Summarised: (1 * new alpha) + (0 * old alpha)
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

	printf("Create Blending State\n");
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;							// Alternative to calculation is to use logical operation
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;


	// -- PIPELINE LAYOUT -- (TODO: Apply Future Descriptors Layouts)
	printf("Create Pipeline Layout\n");
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	// Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}


	// -- DEPTH STENCIL TESTING --
	//	TODO: Set up depth stencil testing
	printf("Create Depth Stencil Testing\n");

	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;											// Number of shaders
	pipelineCreateInfo.pStages = shaderStages;									// List of shaders
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;				// Vertex Input State
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;					// Assembly State
	pipelineCreateInfo.pViewportState = &viewPortStateCreateInfo;				// ViewPort State
	pipelineCreateInfo.pDynamicState = nullptr;									// Dynamic State
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;		// Fragment/Resterization State
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;			// Multisamples State
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;			// Color Blend State
	pipelineCreateInfo.pDepthStencilState = nullptr;							// Depth Stencil State
	pipelineCreateInfo.layout = pipelineLayout;									// Pipeline Layout should use
	pipelineCreateInfo.renderPass = renderPass;									// Render Pass description the pipeline compatible with
	pipelineCreateInfo.subpass = 0;												// Subpass of Render Pass to use with pipeline

	// Pipeline Derivatives : Can create multiple pipelines thet derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;						// Existing pipeline for derive from ...
	pipelineCreateInfo.basePipelineIndex = -1;									// or index of pipeline being created to derive from (in case creating multiple at once)

	// Create Graphics Pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Graphics Pipeline!");
	}

	printf("Graphics Pipeline created successful\n");

	printf("Destroy shader modules:\n");
	// Destroy shader modules, no longer needed after Pipeline created
	printf("Destroy Vertex shader module\n");
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	printf("Destroy Fragment shader module\n");
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

	printf("----------------------------------\n");
}

void VulkanRenderer::CreateFrameBuffers()
{
	printf("STAGE: Create FrameBuffers\n\n");
	// Resize framebuffers count to equal swap chain image count
	swapChainFrameBuffers.resize(swapChainImages.size());

	// Create a framebuffer for each swap chain image
	for (size_t i = 0; i < swapChainFrameBuffers.size(); i++)
	{
		std::array<VkImageView, 1> attachments = {
			swapChainImages[i].imageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;										// Render Pass layout the FrameBuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());	// Count of attachments
		framebufferCreateInfo.pAttachments = attachments.data();							// List of attachments (1:1 with Render Pass)
		framebufferCreateInfo.width = swapChainExtent.width;								// FrameBuffer width
		framebufferCreateInfo.height = swapChainExtent.height;								// FrameBuffer height
		framebufferCreateInfo.layers = 1;													// FrameBuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFrameBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create a FrameBuffer!");
		}
	}

	printf("FrameBuffers created successful\n");
	printf("----------------------------------\n");
}

void VulkanRenderer::CreateCommandPool()
{
	printf("STAGE: Create Command Pool \n\n");
	// Get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;		// Queue Family type buffers from this command pool will use

	// Create a Graphics Queue Family Command Pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Command Pool!");
	}

	printf("Command Pool created successful\n");
	printf("----------------------------------\n");
}

void VulkanRenderer::CreateCommandBuffers()
{
	printf("STAGE: Create Command Buffers \n\n");

	// Resize command buffer count to have one for each framebuffer
	commandBuffers.resize(swapChainFrameBuffers.size());

	VkCommandBufferAllocateInfo cbAllocateInfo = {};
	cbAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocateInfo.commandPool = graphicsCommandPool;
	cbAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;					// VK_COMMAND_BUFFER_LEVEL_PRIMARY : Buffer you submit directly to queue. Cant be called by other buffers.
																			// VK_COMMAND_BUFFER_LEVEL_SECONDARY : Buffer can't be called directly. Can be called from other buffers via "vkCmdExecuteCommands" when recording commands in primary buffer
	cbAllocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	// Allocate commands buffers and place handlers in array of buffers
	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocateInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}

	printf("Command Buffers created successful\n");
	printf("----------------------------------\n");
}

void VulkanRenderer::CreateSynchronisation()
{
	imageAvaible.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvaible[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Semaphores and/or Fences!");
		}
	}
}

void VulkanRenderer::RecordCommands()
{
	printf("STAGE: Record Commands \n\n");
	// Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmitted when it has already been submitted and awaiting execution

	// Information how te begin Render Pass (only need in graphical application)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;								// Render Rass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };							// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;					// Size of region to run render pass on (starting at offset)
	VkClearValue clearValues[] = {
		{0.0f, 0.0f, .0f, 1.0f},
	};
	renderPassBeginInfo.pClearValues = clearValues;								// List of clear values (TODO: Depth Attachment Clear)
	renderPassBeginInfo.clearValueCount = 1;									// Count of clear values

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		renderPassBeginInfo.framebuffer = swapChainFrameBuffers[i];

		// Start recording command in command buffer
		VkResult result = vkBeginCommandBuffer(commandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a Command Buffer!");
		}

		// Begin Render Pass
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind Pipeline to be used in render pass
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertexBuffers[] = { firstMesh.GetVertexBuffer() };					// Buffers to bind
		VkDeviceSize offsets[] = { 0 };												// Offsets into buffers being bound
		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);	// Command to bind vertex buffer before drawing with them

		// Execute Pipeline
		vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.GetVertexCount()), 1, 0, 0);

		// End Render Pass
		vkCmdEndRenderPass(commandBuffers[i]);
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to end recording a Command Buffer!");
		}
	}

	printf("Record Commands successful\n");
	printf("----------------------------------\n");
}

void VulkanRenderer::GetPhysicalDevice()
{
	printf("STAGE: Create Physical Device\n\n");

	// Enumerate physical devies the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	printf("GPU Count: %x\n", deviceCount);

	// Throw if no device avaible, then none support Vulkan!
	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPUs that support Vulan Instance!");
	}

	// Get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	printf("GPU List:\n");
	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	// Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
	printf("Device selected: %s\n", deviceProperties.deviceName);
	printf("Physical Device created successful\n");
	printf("----------------------------------\n");
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

bool VulkanRenderer::CheckValidationLayerSupport()
{
	// Get number of validation layers to create vector of appropriate size
	uint32_t validationLayerCount;
	vkEnumerateInstanceLayerProperties(&validationLayerCount, nullptr);

	// Check if no validation layers found AND we want at least 1 layer
	if (validationLayerCount == 0 && validationLayers.size() > 0)
	{
		return false;
	}

	std::vector<VkLayerProperties> availableLayers(validationLayerCount);
	vkEnumerateInstanceLayerProperties(&validationLayerCount, availableLayers.data());

	// Check if given Validation Layer is in list of given Validation Layers
	for (const auto& validationLayer : validationLayers)
	{
		bool hasLayer = false;
		for (const auto& availableLayer : availableLayers)
		{
			if (strcmp(validationLayer, availableLayer.layerName) == 0)
			{
				hasLayer = true;
				break;
			}
		}

		if (!hasLayer)
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

	printf("\t%s\n", deviceProperties.deviceName);

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
	viewCreateInfo.subresourceRange.layerCount = 1;						// Number of array levels to view TODO ?

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
