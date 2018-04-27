// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ANIMATION_MANAGER
#define _ANIMATION_MANAGER
#pragma once

#include "GlobalAnimationHeaderAIM.h"
#include "GlobalAnimationHeaderCAF.h"
#include <mutex>

class CAnimationManager
{
public:

	void Clear()
	{
		m_arrGlobalAIM.clear();
		m_arrGlobalAnimations.clear();
	}

	bool AddAIMHeaderOnly(const GlobalAnimationHeaderAIM& header);
	bool HasAIMHeader(const GlobalAnimationHeaderAIM& header) const;
	bool AddCAFHeaderOnly(const GlobalAnimationHeaderCAF& header);
	bool HasCAFHeader(const GlobalAnimationHeaderCAF& header) const;

	bool CanBeSkipped();

	bool SaveAIMImage(const char* fileName, FILETIME timeStamp, bool bigEndianFormat);
	bool SaveCAFImage(const char* fileName, FILETIME timeStamp, bool bigEndianFormat);
private:
	bool HasAIM(uint32 pathCRC) const;
	bool HasCAF(uint32 pathCRC) const;

	typedef std::vector<GlobalAnimationHeaderCAF> GlobalAnimationHeaderCAFs;
	GlobalAnimationHeaderCAFs m_arrGlobalAnimations; 
	std::recursive_mutex m_lockCAFs;

	typedef std::vector<GlobalAnimationHeaderAIM> GlobalAnimationHeaderAIMs;
	GlobalAnimationHeaderAIMs m_arrGlobalAIM;
	std::recursive_mutex m_lockAIMs;
};


#endif
