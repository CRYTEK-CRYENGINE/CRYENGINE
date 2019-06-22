// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

#include "VKExtensions.hpp"

namespace NCryVulkan { namespace Extensions 
{
	namespace EXT_debug_marker
	{
		bool                              IsSupported          = false;

		PFN_vkDebugMarkerSetObjectTagEXT  SetObjectTag         = nullptr;
		PFN_vkDebugMarkerSetObjectNameEXT SetObjectName        = nullptr;
		PFN_vkCmdDebugMarkerBeginEXT      CmdDebugMarkerBegin  = nullptr;
		PFN_vkCmdDebugMarkerEndEXT        CmdDebugMarkerEnd    = nullptr;
		PFN_vkCmdDebugMarkerInsertEXT     CmdDebugMarkerInsert = nullptr;
	}

	namespace EXT_rasterization_order
	{
		bool                              IsSupported          = false;
	}

	void Init(CDevice* pDevice, const std::vector<const char*>& loadedExtensions)
	{
		for (auto extensionName : loadedExtensions)
		{
			if (strcmp(extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
			{
				debugMarkerObjectNameMap[pDevice->GetVkDevice()] = DeviceObjPointerNameMap();

				EXT_debug_marker::SetObjectTag         = (PFN_vkDebugMarkerSetObjectTagEXT)  vkGetDeviceProcAddr(pDevice->GetVkDevice(), "vkDebugMarkerSetObjectTagEXT");
				EXT_debug_marker::SetObjectName        = (PFN_vkDebugMarkerSetObjectNameEXT) vkGetDeviceProcAddr(pDevice->GetVkDevice(), "vkDebugMarkerSetObjectNameEXT");
				EXT_debug_marker::CmdDebugMarkerBegin  = (PFN_vkCmdDebugMarkerBeginEXT)      vkGetDeviceProcAddr(pDevice->GetVkDevice(), "vkCmdDebugMarkerBeginEXT");
				EXT_debug_marker::CmdDebugMarkerEnd    = (PFN_vkCmdDebugMarkerEndEXT)        vkGetDeviceProcAddr(pDevice->GetVkDevice(), "vkCmdDebugMarkerEndEXT");
				EXT_debug_marker::CmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)     vkGetDeviceProcAddr(pDevice->GetVkDevice(), "vkCmdDebugMarkerInsertEXT");

				EXT_debug_marker::IsSupported = true;
			}
			else if (strcmp(extensionName, VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME) == 0)
			{
				EXT_rasterization_order::IsSupported = true;
			}
		}
	}

	VkResult SetObjectName(VkDevice device, uintptr_t objectPtr, VkDebugMarkerObjectNameInfoEXT* pNameInfo)
	{
		if (!EXT_debug_marker::IsSupported) return VK_ERROR_EXTENSION_NOT_PRESENT;

		GetDeviceObjPointerNameMap(device)[objectPtr] = pNameInfo->pObjectName;
		return EXT_debug_marker::SetObjectName(device, pNameInfo);
	}

	DeviceObjPointerNameMap& GetDeviceObjPointerNameMap(VkDevice device)
	{
		auto deviceMarkerObjectNameMap = debugMarkerObjectNameMap.find(device);
		CRY_ASSERT(deviceMarkerObjectNameMap != debugMarkerObjectNameMap.end());
		return deviceMarkerObjectNameMap->second;
	}

	std::string GetObjectName(VkDevice device, uintptr_t objectPtr)
	{
		if (!EXT_debug_marker::IsSupported) return "";

		auto deviceMarkerObjectNameMap = GetDeviceObjPointerNameMap(device);

		auto debugName = deviceMarkerObjectNameMap.find(objectPtr);
		if(debugName != deviceMarkerObjectNameMap.end())
			return debugName->second;
		return "";
	}

	void ClearDebugName(VkDevice device, uintptr_t objectPtr)
	{
		if (!EXT_debug_marker::IsSupported) return;

		auto deviceMarkerObjectNameMap = GetDeviceObjPointerNameMap(device);

		auto debugName = deviceMarkerObjectNameMap.find(objectPtr);
		if (debugName != deviceMarkerObjectNameMap.end())
			debugName->second = "";
	}
}}
