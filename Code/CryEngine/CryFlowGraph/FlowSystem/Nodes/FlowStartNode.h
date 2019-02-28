// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWSTARTNODE_H__
#define __FLOWSTARTNODE_H__

#pragma once

#include <CryFlowGraph/IFlowSystem.h>

class CFlowStartNode : public IFlowNode
{
public:
	CFlowStartNode(SActivationInfo*);

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef&, bool);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);
	virtual void         PostSerialize(SActivationInfo*) {};

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	// ~IFlowNode

private:
	bool m_bActivated;

	void SetActivation(SActivationInfo*, bool value);
	bool MayTrigger(SActivationInfo*);
};

#endif
