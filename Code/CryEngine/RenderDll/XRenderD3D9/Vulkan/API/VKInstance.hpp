// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"

namespace NCryVulkan
{

class CDevice;

struct SLayerInfo
{

	VkLayerProperties                  properties;
	std::vector<VkExtensionProperties> extensions;
};

struct SInstanceInfo
{
	std::vector<SLayerInfo> instanceLayers;
	std::vector<VkExtensionProperties> implicitExtensions;
};

struct SPhysicalDeviceInfo
{
	VkPhysicalDevice device;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	std::vector<SLayerInfo> devicelayers;
	std::vector<VkExtensionProperties> implicitExtensions;
	std::array<VkFormatProperties, VK_FORMAT_RANGE_SIZE> formatProperties;
};

//CInstance handles the initalization of the Vulkan API and instance, querying the hardware capabilities and creating a device and surface
class CInstance
{

public:
	CInstance();
	~CInstance();

#if defined(USE_SDL2_VIDEO)
	static bool CreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND * pHandle);
	static void DestroySDLWindow(HWND kHandle);
#endif //defined(USE_SDL2_VIDEO)

	bool Initialize(const char* appName, uint32_t appVersion, const char* engineName, uint32_t engineVersion);

	size_t GetPhysicalDeviceCount() const { return m_physicalDevices.size(); }

	_smart_ptr<CDevice> CreateDevice(size_t physicalDeviceIndex);

	//platform dependant function to create platform independant VkSurfaceKHR handle
	VkResult CreateSurface(const SSurfaceCreationInfo& info, VkSurfaceKHR* surface);
	void DestroySurface(VkSurfaceKHR surface);

private:
	VkResult InitializeInstanceLayerInfos();
	VkResult InitializeInstanceExtensions(const char* layerName, std::vector<VkExtensionProperties>& extensions);
	VkResult InitializeInstance(const char* appName, uint32_t appVersion, const char* engineName, uint32_t engineVersion);
	VkResult InitializeDebugLayerCallback();

	VkResult InitializePhysicalDeviceInfos();
	VkResult InitializePhysicalDeviceLayerInfos(SPhysicalDeviceInfo& info);
	VkResult InitializePhysicalDeviceExtensions(VkPhysicalDevice& device, const char* layerName, std::vector<VkExtensionProperties>& extensions);

	//Gather* functions fill in the names of the layers & extensions we want to enable. For now, the only debug layer queried is VK_LAYER_LUNARG_standard_validation, could potentially try to
	//query specific ones if the standard validation layer not present.
	void GatherInstanceLayersToEnable();
	void GatherInstanceExtensionsToEnable();
	void GatherPhysicalDeviceLayersToEnable();
	void GatherPhysicalDeviceExtensionsToEnable();

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugLayerCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, 
		uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);

private:
	struct SExtensionInfo
	{
		SExtensionInfo(const char* extensionName, bool isRequired)
			: name(extensionName)
			, bRequired(isRequired)
		{}

		const char* name;
		bool        bRequired;
	};

	SInstanceInfo m_instanceInfo;
	CHostAllocator m_Allocator;
	std::vector<SPhysicalDeviceInfo> m_physicalDevices;

	std::vector<const char*>    m_enabledInstanceLayers;
	std::vector<const char*>    m_enabledInstanceExtensions;
	std::vector<const char*>    m_enabledPhysicalDeviceLayers;
	std::vector<SExtensionInfo> m_enabledPhysicalDeviceExtensions;

	VkInstance                  m_instanceHandle;
	VkDebugReportCallbackEXT    m_debugLayerCallbackHandle;

};

}
