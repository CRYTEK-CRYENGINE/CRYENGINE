// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 3:06:2009   Created by Benito G.R.
*************************************************************************/

#pragma once

#ifndef SCREEN_FADER_NODE_H
#define SCREEN_FADER_NODE_H

#include "GameFlowBaseNode.h"

namespace EngineFacade
{
	struct IGameEnvironment;
}

class CFlowNode_ScreenFader : public CGameFlowBaseNode
{
public:
	static const SInputPortConfig inputPorts[];

	CFlowNode_ScreenFader(IGameEnvironment& environment, SActivationInfo* activationInformation);

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessActivateEvent(SActivationInfo* activationInformation);
	virtual void GetMemoryStatistics(ICrySizer* sizer);

	enum EInputPorts
	{
		eInputPorts_FadeIn,
		eInputPorts_FadeOut,
		eInputPorts_FadeInTime,
		eInputPorts_FadeOutTime,
		eInputPorts_FadeColor,
		eInputPorts_UseCurrentColor,
		eInputPorts_Texture,
		eInputPorts_Count,
	};

private:
	IGameEnvironment& m_environment;
};

#endif 