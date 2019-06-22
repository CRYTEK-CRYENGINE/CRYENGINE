// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"

namespace NCryVulkan
{
	namespace Extensions
	{
		namespace EXT_debug_marker
		{
			extern bool                              IsSupported;

			extern PFN_vkDebugMarkerSetObjectTagEXT  SetObjectTag;
			extern PFN_vkDebugMarkerSetObjectNameEXT SetObjectName;
			extern PFN_vkCmdDebugMarkerBeginEXT      CmdDebugMarkerBegin;
			extern PFN_vkCmdDebugMarkerEndEXT        CmdDebugMarkerEnd;
			extern PFN_vkCmdDebugMarkerInsertEXT     CmdDebugMarkerInsert;
		}

		namespace EXT_rasterization_order
		{
			extern bool                              IsSupported;
		}

		void Init(CDevice* pDevice, const std::vector<const char*>& loadedExtensions);

		typedef                  std::unordered_map<uintptr_t, std::string>   DeviceObjPointerNameMap;
		static                   std::map<VkDevice, DeviceObjPointerNameMap> debugMarkerObjectNameMap;
		VkResult                 SetObjectName              (VkDevice device, uintptr_t objectPtr, VkDebugMarkerObjectNameInfoEXT* pNameInfo);
		DeviceObjPointerNameMap& GetDeviceObjPointerNameMap (VkDevice device);
		std::string              GetObjectName              (VkDevice device, uintptr_t objectPtr);
		void                     ClearDebugName             (VkDevice device, uintptr_t objectPtr);
	}
}