// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKInstance.hpp"
#include "VKDevice.hpp"
#include "VKExtensions.hpp"

namespace NCryVulkan
{

CInstance::CInstance()
	: m_instanceHandle(VK_NULL_HANDLE)
	, m_debugLayerCallbackHandle(VK_NULL_HANDLE)
{
	

}

CInstance::~CInstance()
{
	if (m_debugLayerCallbackHandle != VK_NULL_HANDLE)
	{
		if (auto pDestroyCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instanceHandle, "vkDestroyReportCallbackEXT")))
		{
			pDestroyCallback(m_instanceHandle, m_debugLayerCallbackHandle, nullptr);
		}
	}

	if (m_instanceHandle != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instanceHandle, NULL);
	}
}

#if defined(USE_SDL2_VIDEO)
#include <SDL_video.h>

bool CreateSDLWindowContext(SDL_Window*& kWindowContext, const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen)
{
	uint32 uWindowFlags = 0;
	if (bFullScreen)
	{
#if CRY_PLATFORM_LINUX
		// SDL2 does not handle correctly fullscreen windows with non-native display mode on Linux.
		// It is easier to render offscreen at internal resolution and blit-scale it to native resolution at the end.
		// There is a bug with fullscreen on Linux: https://bugzilla.libsdl.org/show_bug.cgi?id=2373, which makes
		// the fullscreen window leave a black spot in the top.
		uWindowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE;
#else
		uWindowFlags |= SDL_WINDOW_FULLSCREEN;
#endif
	}

	uWindowFlags |= SDL_WINDOW_VULKAN;

//	SDL_VideoInit("dummy");
	kWindowContext = SDL_CreateWindow(szTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, uWidth, uHeight, uWindowFlags);
#if !defined(_DEBUG)
	SDL_SetWindowGrab(kWindowContext, SDL_TRUE);
#endif
	if (!kWindowContext)
	{
		VK_ERROR("Failed to create SDL Window %s", SDL_GetError());
		return false;
	}

	// These calls fail, check SDL source (must be >0)
	//SDL_SetWindowMinimumSize(kWindowContext, 0, 0);
	//SDL_SetWindowMaximumSize(kWindowContext, 0, 0);

	if (bFullScreen)
	{
		SDL_DisplayMode kDisplayMode;
		SDL_zero(kDisplayMode);

		if (SDL_SetWindowDisplayMode(kWindowContext, &kDisplayMode) != 0)
		{
			VK_ERROR("Failed to set display mode: %s", SDL_GetError());
			return false;
		}

		SDL_ShowWindow(kWindowContext);
	}

	return true;
}

bool CInstance::CreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND* pHandle)
{
	SDL_Window* pWindowContext;
	if (!CreateSDLWindowContext(pWindowContext, szTitle, uWidth, uHeight, bFullScreen))
		return false;
	*pHandle = reinterpret_cast<HWND>(pWindowContext);
	return true;
}

void CInstance::DestroySDLWindow(HWND kHandle)
{
	SDL_Window* pWindowContext = reinterpret_cast<SDL_Window*>(kHandle);
	if (pWindowContext)
		SDL_DestroyWindow(pWindowContext);
}

#endif //defined(USE_SDL2_VIDEO)

_smart_ptr<CDevice> CInstance::CreateDevice(size_t physicalDeviceIndex)
{
	VK_ASSERT(physicalDeviceIndex < m_physicalDevices.size() && "Bad device index");
	const SPhysicalDeviceInfo& pDeviceInfo = m_physicalDevices[physicalDeviceIndex];
	VkAllocationCallbacks allocationCallbacks;
	m_Allocator.GetCpuHeapCallbacks(allocationCallbacks);

	// filter out extensions which are not available
	std::vector<const char*> extensions;
	for (const auto& requestedExtension : m_enabledPhysicalDeviceExtensions)
	{
		int i;
		for (i=0; i < pDeviceInfo.implicitExtensions.size(); ++i)
		{
			const VkExtensionProperties& availableExtension = pDeviceInfo.implicitExtensions[i];

			if (strcmp(availableExtension.extensionName, requestedExtension.name) == 0)
			{
				extensions.push_back(requestedExtension.name);
				break;
			}
		}

		if (i == pDeviceInfo.implicitExtensions.size() && requestedExtension.bRequired)
		{
			VK_ERROR("Failed to load extension '%s'", requestedExtension.name);
			return nullptr;
		}
	}

	auto device = CDevice::Create(&pDeviceInfo, &allocationCallbacks, m_enabledPhysicalDeviceLayers, extensions);
	Extensions::Init(device.get(), extensions);

	return device;
}

VkResult CInstance::CreateSurface(const SSurfaceCreationInfo& info, VkSurfaceKHR* surface)
{
#ifdef CRY_PLATFORM_WINDOWS
	VkWin32SurfaceCreateInfoKHR createInfo = {};

	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.hinstance = info.appHandle;
	createInfo.hwnd = info.windowHandle;

	return vkCreateWin32SurfaceKHR(m_instanceHandle, &createInfo, NULL, surface);
#elif defined(CRY_PLATFORM_ANDROID)
	VkAndroidSurfaceCreateInfoKHR createInfo = {};

	createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.window = info.pNativeWindow;

	return vkCreateAndroidSurfaceKHR(m_instanceHandle, &createInfo, NULL, surface);
#elif defined(CRY_PLATFORM_LINUX)
#error  "Not implemented!""
#endif
}

void CInstance::DestroySurface(VkSurfaceKHR surface)
{
	
}

bool CInstance::Initialize(const char* appName, uint32_t appVersion, const char* engineName, uint32_t engineVersion)
{
	InitializeInstanceLayerInfos();
	GatherInstanceLayersToEnable();
	GatherInstanceExtensionsToEnable();
	if (InitializeInstance(appName, appVersion, engineName, engineVersion) != VK_SUCCESS)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Vulkan instance initialization failed! Please make sure your GPU supports Vulkan.");
		return false;
	}

	InitializeDebugLayerCallback();
	InitializePhysicalDeviceInfos();
	VK_ASSERT(m_physicalDevices.size() > 0);

	GatherPhysicalDeviceLayersToEnable();
	GatherPhysicalDeviceExtensionsToEnable();
	return true;
}

VkResult CInstance::InitializeInstanceLayerInfos()
{
	uint32_t layerCount;
	VkLayerProperties* properties = nullptr;
	VkResult res;

	do
	{
		res = vkEnumerateInstanceLayerProperties(&layerCount, NULL);

		if (res != VK_SUCCESS)
		{
			return res;
		}

		if (layerCount == 0)
		{
			return VK_SUCCESS;
		}

		properties = static_cast<VkLayerProperties*>(realloc(properties, layerCount * sizeof(VkLayerProperties)));

		res = vkEnumerateInstanceLayerProperties(&layerCount, properties);
	}
	while (res == VK_INCOMPLETE);

	//query layer specific extensions
	m_instanceInfo.instanceLayers.resize(layerCount);
	for (size_t i = 0; i < layerCount; ++i)
	{
		m_instanceInfo.instanceLayers[i].properties = properties[i];
		VkResult res = InitializeInstanceExtensions(m_instanceInfo.instanceLayers[i].properties.layerName, m_instanceInfo.instanceLayers[i].extensions);
		if (res != VK_SUCCESS)
		{
			VK_ERROR("Failed to load instance extensions for layer %s", m_instanceInfo.instanceLayers[i].properties.layerName);
		}
	}
	//query implicit extensions
	res = InitializeInstanceExtensions(NULL, m_instanceInfo.implicitExtensions);
	if (res != VK_SUCCESS)
	{
		VK_ERROR("Failed to load implicit layer extensions");
	}



	free(properties);

	return VK_SUCCESS;
}

VkResult CInstance::InitializeInstanceExtensions(const char* layerName, std::vector<VkExtensionProperties>& extensions)
{
	VkExtensionProperties* extensionsList;
	uint32_t extensionCount;
	VkResult res;

	do
	{
		res = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, NULL);

		if (res != VK_SUCCESS)
		{
			return res;
		}

		if (extensionCount == 0)
		{
			return VK_SUCCESS;
		}

		extensions.resize(extensionCount);
		extensionsList = extensions.data();
		res = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, extensionsList);
	}
	while (res == VK_INCOMPLETE);

	return res;
}

VkResult CInstance::InitializeInstance(const char* appName, uint32_t appVersion, const char* engineName, uint32_t engineVersion)
{
	VkApplicationInfo applicationInfo = {};

	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = NULL;
	applicationInfo.pApplicationName = appName;
	applicationInfo.applicationVersion = appVersion;
	applicationInfo.pEngineName = engineName;
	applicationInfo.engineVersion = engineVersion;
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceInfo = {};

	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = NULL;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &applicationInfo;
	instanceInfo.enabledLayerCount = m_enabledInstanceLayers.size();
	instanceInfo.ppEnabledLayerNames = (!m_enabledInstanceLayers.empty()) ? m_enabledInstanceLayers.data() : NULL;
	instanceInfo.enabledExtensionCount = m_enabledInstanceExtensions.size();
	instanceInfo.ppEnabledExtensionNames = (!m_enabledInstanceExtensions.empty()) ? m_enabledInstanceExtensions.data() : NULL;

	VkResult res = vkCreateInstance(&instanceInfo, NULL, &m_instanceHandle);
	if (res == VK_ERROR_LAYER_NOT_PRESENT)
	{
		// (debug) layer not installed, don't consider fatal.
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = nullptr;
		res = vkCreateInstance(&instanceInfo, NULL, &m_instanceHandle);
		if (res == VK_SUCCESS)
		{
			// In case we succeed now, at least put this in the log.
			// If not, use the below error handling.
			CryLogAlways("vkCreateInstance: Discarded %" PRISIZE_T " layers during Vulkan initialization", m_enabledInstanceLayers.size());
		}
	}
	VK_ASSERT(res == VK_SUCCESS);

	return res;
}

VkResult CInstance::InitializePhysicalDeviceInfos()
{
	uint32_t gpuCount(0);
	VkResult res = vkEnumeratePhysicalDevices(m_instanceHandle, &gpuCount, NULL);
	VK_ASSERT(gpuCount);

	std::vector<VkPhysicalDevice> gpus;
	gpus.resize(gpuCount);

	res = vkEnumeratePhysicalDevices(m_instanceHandle, &gpuCount, gpus.data());
	VK_ASSERT(res == VK_SUCCESS);

	m_physicalDevices.resize(gpuCount);

	//iterate over available gpus and query their info
	for (size_t i = 0; i < gpus.size(); ++i)
	{
		uint32_t queueCount;
		SPhysicalDeviceInfo& info = m_physicalDevices[i];
		info.device = gpus[i];

		// Fetch queue family properties for this GPU
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueCount, NULL);
		info.queueFamilyProperties.resize(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueCount, info.queueFamilyProperties.data());

		// Fetch gpu & gpu memory properties
		vkGetPhysicalDeviceProperties(gpus[i], &info.deviceProperties);
		vkGetPhysicalDeviceMemoryProperties(gpus[i], &info.deviceMemoryProperties);

		// Fetch physical device features
		vkGetPhysicalDeviceFeatures(info.device, &info.deviceFeatures);

		// Fetch physical device format support
		for (VkFormat format = VK_FORMAT_BEGIN_RANGE; format != VK_FORMAT_END_RANGE; format = VkFormat(format + 1))
			vkGetPhysicalDeviceFormatProperties(info.device, format, &info.formatProperties[format]);

		//fetch device layers & extensions
		VkResult result = InitializePhysicalDeviceLayerInfos(info);
		if (result != VK_SUCCESS)
		{
			res = result;
		}
	}

	return res;
}

VkResult CInstance::InitializePhysicalDeviceLayerInfos(SPhysicalDeviceInfo& info)
{
	VkResult finalResult = VK_SUCCESS;

	// Explicit
	{
		uint32_t layerCount;
		VkLayerProperties* properties = nullptr;
		VkResult result;

		do
		{
			result = vkEnumerateDeviceLayerProperties(info.device, &layerCount, NULL);

			if (result != VK_SUCCESS)
			{
				break;
			}

			if (layerCount == 0)
			{
				break;
			}

			properties = static_cast<VkLayerProperties*>(realloc(properties, layerCount * sizeof(VkLayerProperties)));

			result = vkEnumerateDeviceLayerProperties(info.device, &layerCount, properties);
		}
		while (result == VK_INCOMPLETE);

		if (result == VK_SUCCESS)
		{
			std::vector<SLayerInfo>& layers = info.devicelayers;
			layers.resize(layerCount);

			for (size_t i = 0; i < layerCount; ++i)
			{
				layers[i].properties = properties[i];
				VkResult result = InitializePhysicalDeviceExtensions(info.device, layers[i].properties.layerName, layers[i].extensions);
				if (result != VK_SUCCESS)
				{
					VK_ERROR("Failed to load device extensions for layer %s for device %s", layers[i].properties.layerName, info.deviceProperties.deviceName);
					finalResult = finalResult == VK_SUCCESS && result == VK_SUCCESS ? VK_SUCCESS : result;
				}
			}
		}

		free(properties);
	}

	// Implicit
	{
		VkResult result = InitializePhysicalDeviceExtensions(info.device, NULL, info.implicitExtensions);
		if (result != VK_SUCCESS)
		{
			VK_ERROR("Failed to load implicit device extensions");
			finalResult = finalResult == VK_SUCCESS && result == VK_SUCCESS ? VK_SUCCESS : result;
		}
	}

	return finalResult;
}

VkResult CInstance::InitializePhysicalDeviceExtensions(VkPhysicalDevice& device, const char* layerName, std::vector<VkExtensionProperties>& extensions)
{
	VkExtensionProperties* extensionsList;
	uint32_t extensionCount;
	VkResult res;

	do
	{
		res = vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, NULL);

		if (res != VK_SUCCESS)
		{
			return res;
		}

		if (extensionCount == 0)
		{
			return VK_SUCCESS;
		}

		extensions.resize(extensionCount);
		extensionsList = extensions.data();
		res = vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, extensionsList);
	}
	while (res == VK_INCOMPLETE);

	return res;
}

void CInstance::GatherInstanceLayersToEnable()
{
	if (CRendererCVars::CV_r_EnableDebugLayer)
	{
		m_enabledInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	}
}

void CInstance::GatherInstanceExtensionsToEnable()
{
	//platform independent extensions
	m_enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

	//platform dependent extensions
#ifdef CRY_PLATFORM_WINDOWS
	m_enabledInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(CRY_PLATFORM_ANDROID)
	m_enabledInstanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(CRY_PLATFORM_LINUX)
	m_enabledInstanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

	if (CRendererCVars::CV_r_EnableDebugLayer)
	{
		m_enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
}

void CInstance::GatherPhysicalDeviceLayersToEnable()
{
	if (CRendererCVars::CV_r_EnableDebugLayer)
	{
		m_enabledPhysicalDeviceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	}
}

void CInstance::GatherPhysicalDeviceExtensionsToEnable()
{
	m_enabledPhysicalDeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true);

#if !defined(_RELEASE) && VK_EXT_debug_marker
	m_enabledPhysicalDeviceExtensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false);
#endif

}

VKAPI_ATTR VkBool32 VKAPI_CALL CInstance::DebugLayerCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj,
	size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData) 
{
	if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT))
	{
		CryLog("[Vulkan DebugLayer, %s, %d]: %s", layerPrefix, code, msg);
	}
	return VK_FALSE;
}

VkResult CInstance::InitializeDebugLayerCallback()
{
	if (CRendererCVars::CV_r_EnableDebugLayer)
	{
		if (auto pCreateCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instanceHandle, "vkCreateDebugReportCallbackEXT")))
		{
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
			createInfo.pfnCallback = CInstance::DebugLayerCallback;

#if 0
			// TheoM TODO: Add back these flags once we have fixed all validation errors
			createInfo.flags |=
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_WARNING_BIT_EXT;
#endif

			if (pCreateCallback(m_instanceHandle, &createInfo, nullptr, &m_debugLayerCallbackHandle) == VK_SUCCESS)
				return VK_SUCCESS;
		}

		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Failed to initialize debug layer report callback");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	return VK_SUCCESS;
}

}
