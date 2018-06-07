#define GLFW_INCLUDE_VULKAN
//#define GLFW_EXPOSE_NATIVE_X11
//#define GLFW_EXPOSE_NATIVE_GLX

#include <GLFW/glfw3.h>
//#include <GLFW/glfw3native.h>

//#include <xcb/xcb.h>
//#include <vulkan/vulkan_xcb.h>

#include <string.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation",
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDebug
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamiliyIndicies {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

static VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugReportCallbackEXT(VkInstance instance,
	VkDebugReportCallbackEXT callback,
	const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, callback, pAllocator);
	}
}

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::runtime_error("fail to open file!");
	}
	size_t filesize = (size_t)file.tellg();
	std::vector<char> buffer(filesize);
	file.seekg(0);
	file.read(buffer.data(), filesize);
	file.close();

	return buffer;
}

class Triangle {
public:
	void run(){
		initWindow();
		initVulkan();
		mainloop();
		cleanup();
	}
private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugReportCallbackEXT callback;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device; //logical device

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;

	void initWindow();

	void createInstance();

	bool checkValidationLayerSupport();

	std::vector<const char*> getRequiredExtenstions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
#ifndef Ndebug
		for (const auto& extension : extensions) {
			std::cout << extension << std::endl;
		}
#endif
		return extensions;
	}

	void initVulkan();

	void createSurface();

	void pickPhysicalDevice();
	/*-------------Logical device--------------*/
	void createLogicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamiliyIndicies findQueueFamilies(VkPhysicalDevice device);
	bool checkDeviceExtensionSupported(VkPhysicalDevice device);
	int rateDeviceSuitability(VkPhysicalDevice device);
	/*-------------Callback--------------*/
	void setupDebugCallback();
	/*-------------SwapChain-------------*/
	/*
	->Surface format (color depth)
	->Presentation mode (conditions for "swapping" images to the screen)
	->Swap extent (resolution of images in swap chain)
	*/
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapsurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void createSwapChain();
	void createImageViews();

	/*----------graphics pipeline-----------------*/
	void createGraphicsPipeline();
	/*-------mainloop-------------*/
	void mainloop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT	objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData) {

		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

};

void Triangle::initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void Triangle::initVulkan() {
	createInstance();
	setupDebugCallback();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createGraphicsPipeline();
}

void Triangle::createInstance() {
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validate layer requested, but not available !");
	}

	/*--------------appInfo Initiation--------------*/
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "My Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 1);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	/*--------------CreateInfo Initiation--------------*/
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	/*--------------create extensions--------------*/
	auto extensions = getRequiredExtenstions();
#ifndef Ndebug
	std::cout << "Instance extensions count: " << extensions.size() << std::endl;
#endif
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("fail to create instance!");
	}
	/*--------------Print Extensions--------------*/
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> print_extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, print_extensions.data());
#ifndef Ndebug
	std::cout << "Instance extensions:" << std::endl;

	for (const auto& extension : print_extensions) {
		std::cout << "\t" << extension.extensionName << std::endl;
	}
#endif
}

bool Triangle::checkValidationLayerSupport() {
	uint32_t layerCount;

	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layername : validationLayers) {
		bool layerfound = false;
		for (const auto& layerProperties : availableLayers) {
			if (!strcmp(layername, layerProperties.layerName)){
#ifndef Ndebug
				std::cout << layername << " found!" << std::endl;
#endif
				layerfound = true;
				break;
			}
		}
		if (!layerfound)
			return false;
	}
	return true;
}

std::vector<const char*> getRequiredExtenstions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
#ifndef Ndebug
	std::cout << "Required Instance Extensions:" << std::endl;
	for (const auto& extension : extensions) {
		std::cout << extension << std::endl;
	}
#endif
	return extensions;
}

void Triangle::setupDebugCallback() {
	if (!enableValidationLayers)
		return;
	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;
	createInfo.pUserData = (void *)"My first Triangle";
	VkResult result;
	if ((result = CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback)) != VK_SUCCESS) {
		std::cerr << result << std::endl;
		throw std::runtime_error("failed to set up debug callback!");
	}
}

int Triangle::rateDeviceSuitability(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	int score = 0;

	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);


	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	score += deviceProperties.limits.maxImageDimension2D;
#ifndef Ndebug
	std::cout << "Device Type: " << deviceProperties.deviceType << " Score: " << score << std::endl;
#endif
	if (!deviceFeatures.geometryShader || !isDeviceSuitable(device))
		return 0;

	return score;
}

void Triangle::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("fail to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
#ifndef Ndebug
	std::cout << "GPUs: " << deviceCount << std::endl;
#endif
	std::multimap<int, VkPhysicalDevice> candidates;
	for (const auto& device : devices) {
		int score = rateDeviceSuitability(device);
		candidates.insert(std::make_pair(score, device));

	}


	if (candidates.rbegin()->first > 0) {
#ifndef Ndebug
		std::cout << "Candidate score: " << (candidates.rbegin()->first) << std::endl;
#endif
		physicalDevice = candidates.rbegin()->second;
#ifndef Ndebug
		std::cout << "physicalDevice: " << physicalDevice << std::endl;
#endif
	}
	else {
		throw std::runtime_error("fail to find a suitable GPU!");
	}
}

bool Triangle::isDeviceSuitable(VkPhysicalDevice device) {
	QueueFamiliyIndicies indices = findQueueFamilies(device);
	bool extensionSupported = checkDeviceExtensionSupported(device);
	bool swapChainAdequate = false;

	/* only try to query for swap chain support after verifying that the extension is available */
	if (extensionSupported) {
		SwapChainSupportDetails details = querySwapChainSupport(device);
		swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
	}

	return indices.isComplete() && extensionSupported && swapChainAdequate;
}

bool Triangle::checkDeviceExtensionSupported(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

#ifndef Ndebug
	std::cout << "Device Extensions: " << std::endl;
#endif
	for (const auto& extension : availableExtensions) {
#ifndef Ndebug
		std::cout << "\t" << extension.extensionName << std::endl;
#endif
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

QueueFamiliyIndicies Triangle::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamiliyIndicies indices;
	uint32_t queueFamilyCount;
	VkBool32 presentSupport = false;

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (queueFamily.queueCount > 0 && presentSupport) {
			indices.presentFamily = i;
		}

		if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.graphicsFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}
		i++;
	}
	return indices;
}

void Triangle::createLogicalDevice() {
	QueueFamiliyIndicies indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };
	std::cout << "Queuefamily indices, graphics: " << indices.graphicsFamily << " present: " << indices.presentFamily << std::endl;
	float queuePriority = 1.0f;
	for (int queFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("fail to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}

void Triangle::createSurface() {
	/*
	VkXcbSurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	createInfo.window= glfwGetX11Window(window);
	createInfo.connection = xcb_connect(nullptr,nullptr);
	*/
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("fail to create window surface!");
	}
}

SwapChainSupportDetails Triangle::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;
	uint32_t formatCount;
	uint32_t presentModeCount;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
#ifndef Ndebug
		std::cout << "supported Formats ColorSpace: " << std::endl;
		for (const auto& format : details.formats) {
			std::cout << "\t" << format.format << "\t" << format.colorSpace << std::endl;
		}
#endif
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
#ifndef Ndebug
		std::cout << "supported presentModes: " << std::endl;
		for (const auto& presentMode : details.presentModes) {
			std::cout << presentMode << std::endl;
		}
#endif
	}
	return details;
}

VkSurfaceFormatKHR Triangle::chooseSwapsurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM
			&& availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}

	return availableFormats[0];
}

VkPresentModeKHR Triangle::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	/*	VK_PRESENT_MODE_IMMEDIATE_KHR may cause tearing
	VK_PRESENT_MODE_FIFO_KHR The moment that the display is refreshed is known as "vertical blank".
	VK_PRESENT_MODE_FIFO_RELAXED_KHR This may result in visible tearing.
	VK_PRESENT_MODE_MAILBOX_KHR *Preferred* This mode can be used to implement triple buffering
	*/
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}
	return bestMode;
}

VkExtent2D Triangle::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
#ifndef Ndebug
	std::cout << "CurrentExtent: " << capabilities.currentExtent.width << "*" << capabilities.currentExtent.height << std::endl;
#endif
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		VkExtent2D acturalExtent = { WIDTH, HEIGHT };
		acturalExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, acturalExtent.height));
		acturalExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, acturalExtent.width));

		return acturalExtent;
	}
}

void Triangle::createSwapChain() {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapsurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	/* check image count for tripple buffering */
	/* maxImageCount = 0 means no limits */
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
#ifndef Ndebug
	std::cout << "minImageCount: " << swapChainSupport.capabilities.minImageCount << "\tmaxImageCount: "
		<< swapChainSupport.capabilities.maxImageCount << std::endl;
#endif
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}


	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamiliyIndicies indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { static_cast<uint32_t>(indices.graphicsFamily), static_cast<uint32_t>(indices.presentFamily) };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	/* don't care about the color of pixels that are obscured,
	for example because another window is in front of them */
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("fail to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void Triangle::createImageViews() {
	swapChainImageViews.resize(swapChainImages.size());
	for (uint32_t i = 0; i < swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i])) {
			throw std::runtime_error("fail to create imageviews");
		}
	}

}

void Triangle::createGraphicsPipeline() {
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");
}

void Triangle::cleanup() {
	for (auto& imageview : swapChainImageViews) {
		vkDestroyImageView(device, imageview, nullptr);
	}
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers) {
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
	}
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);

	glfwTerminate();
}


int main() {
	Triangle app;
	try {
		app.run();
	}
	catch (const std::runtime_error& e){
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << "my first vulkan triangle" << std::endl;
	return EXIT_SUCCESS;
}