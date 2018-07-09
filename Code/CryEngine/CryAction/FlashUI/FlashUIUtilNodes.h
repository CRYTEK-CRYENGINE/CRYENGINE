// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIUtilNodes.h
//  Version:     v1.00
//  Created:     24/4/2012 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>

// --------------------------------------------------------------
class CFlashUIPlatformNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlashUIPlatformNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIPlatformNode() {}

	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }
	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_Get = 0,
	};
	enum EOutputs
	{
		eO_IsPc = 0,
		eO_IsDurango,
		eO_IsOrbis,
		eO_IsAndroid,
		eO_IsConsole,
	};
};

// --------------------------------------------------------------
class CFlashUIDelayNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlashUIDelayNode(SActivationInfo*) : m_fTime(0.0f) {};

	// IFlowNode
	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIDelayNode(pActInfo); }

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	// ~IFlowNode

private:
	enum EInputs
	{
		eI_Start = 0,
		eI_Delay,
	};
	enum EOutputs
	{
		eO_Done = 0,
	};

	float m_fTime;
};
