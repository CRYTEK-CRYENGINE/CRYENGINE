// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   FlowTrackEventNode.h
   Description: Dynamic node for Track Event logic
   ---------------------------------------------------------------------
   History:
   - 10:04:2008 : Created by Kevin Kirst
   - Apr 11 2017  Modified by Fei Teng

 *********************************************************************/

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryMovie/IMovieSystem.h>

class CFlowTrackEventNode : public CFlowBaseNode<eNCT_Instanced>, public ITrackEventListener
{
public:
	CFlowTrackEventNode(SActivationInfo*);
	CFlowTrackEventNode(CFlowTrackEventNode const& obj) = default;
	CFlowTrackEventNode& operator=(CFlowTrackEventNode const& obj) = default;
	CFlowTrackEventNode(CFlowTrackEventNode&& obj) = default;
	CFlowTrackEventNode& operator=(CFlowTrackEventNode&& obj) = default;
	virtual ~CFlowTrackEventNode();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override;
	virtual void         GetConfiguration(SFlowNodeConfig& config) override;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*) override;
	virtual bool         SerializeXML(SActivationInfo* pActInfo, const XmlNodeRef& root, bool reading) override;
	virtual void         Serialize(SActivationInfo*, TSerialize ser) override;
	virtual void         GetMemoryUsage(class ICrySizer* pSizer) const override;

	// ITrackEventListener
	virtual void OnTrackEvent(IAnimSequence* pSequence, int reason, const char* event, void* pUserData) override;
protected:
	// Add to sequence listener
	void AddListener(SActivationInfo* pActInfo);

private:
	std::vector<string> m_outputStrings;
	std::vector<SOutputPortConfig> m_outputs;

	SActivationInfo    m_actInfo;
	IAnimSequence*     m_pSequence = nullptr;
};
