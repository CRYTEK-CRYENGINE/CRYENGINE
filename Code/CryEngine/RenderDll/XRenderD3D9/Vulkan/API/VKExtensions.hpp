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

		void Init(CDevice* pDevice, const std::vector<const char*>& loadedExtensions);
	}
}