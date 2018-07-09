// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIEventNodes.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUIEventNodes_H__
#define __FlashUIEventNodes_H__

#include "FlashUIBaseNode.h"

class CFlashUIEventSystemFunctionNode
	: public CFlashUIBaseNodeDynPorts
{
public:
	CFlashUIEventSystemFunctionNode(IUIEventSystem* pSystem, string sCategory, const SUIEventDesc* pEventDesc);
	virtual ~CFlashUIEventSystemFunctionNode();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_Send = 0,
	};

	enum EOutputs
	{
		eO_OnEvent = 0,
	};

	IUIEventSystem*                m_pSystem;
	SUIEventDesc                   m_eventDesc;
	uint                           m_iEventId;
	std::vector<SInputPortConfig>  m_inPorts;
	std::vector<SOutputPortConfig> m_outPorts;
};

// --------------------------------------------------------------
class CFlashUIEventSystemEventNode
	: public CFlashUIBaseNodeDynPorts
	  , public IUIEventListener
{
public:
	CFlashUIEventSystemEventNode(IUIEventSystem* pSystem, string sCategory, const SUIEventDesc* pEventDesc);
	virtual ~CFlashUIEventSystemEventNode();

	virtual IFlowNodePtr    Clone(SActivationInfo* pActInfo);
	virtual void            GetConfiguration(SFlowNodeConfig& config);
	virtual void            ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);

	virtual SUIArgumentsRet OnEvent(const SUIEvent& event);
	virtual void            OnEventSystemDestroyed(IUIEventSystem* pEventSystem) { m_pSystem = NULL; }

private:
	void FlushNextEvent(SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_CheckPort = 0,
		eI_CheckValue,
	};

	enum EOutputs
	{
		eO_OnEvent = 0,
	};

	IUIEventSystem*                m_pSystem;
	SUIEventDesc                   m_eventDesc;
	uint                           m_iEventId;
	std::vector<SInputPortConfig>  m_inPorts;
	std::vector<SOutputPortConfig> m_outPorts;

	typedef CUIStack<SUIArguments> TEventStack;
	TEventStack m_events;
};

#endif //#ifndef __FlashUIEventNodes_H__
