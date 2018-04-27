// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Game.h"
#include "Item.h"
#include "Actor.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowItemAction : public CFlowBaseNode<eNCT_Instanced>
{
private:

public:

	CFlowItemAction(SActivationInfo* pActInfo);

	IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void GetMemoryUsage(ICrySizer* s) const;

private:
	void   StartDoneCountDown(SActivationInfo* pActInfo, float time);
	IItem* GetItem(SActivationInfo* pActInfo) const;
	void   Activate(SActivationInfo* pActInfo);
	void   Update(SActivationInfo* pActInfo);

	float m_timerCountDown;
};
