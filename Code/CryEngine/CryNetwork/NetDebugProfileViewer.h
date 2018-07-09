// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETDEBUGPROFILEVIEWER_H__
#define __NETDEBUGPROFILEVIEWER_H__

#pragma once

#include "Config.h"

#if ENABLE_NET_DEBUG_INFO
	#if NET_PROFILE_ENABLE

		#include "NetDebugInfo.h"

class CNetDebugProfileViewer : public CNetDebugInfoSection
{
public:
	CNetDebugProfileViewer(uint32 type);
	virtual ~CNetDebugProfileViewer();

	virtual void Tick(const SNetworkProfilingStats* const pProfilingStats);
	virtual void Draw(const SNetworkProfilingStats* const pProfilingStats);

private:

	void DrawProfileEntry(const SNetProfileStackEntry* entry, int depth, int* line);
	void DrawTree(const SNetProfileStackEntry* root, int depth, int* line);

};

	#endif
#endif // ENABLE_NET_DEBUG_INFO

#endif
